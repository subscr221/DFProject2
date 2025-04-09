#include "auth_system.h"
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

namespace tdoa {
namespace auth {

namespace {
// Helper function to generate a random string
std::string generateRandomString(size_t length) {
    static const std::string chars =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, chars.size() - 1);
    
    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result += chars[dis(gen)];
    }
    return result;
}

// Helper function to hash a password with salt
std::string hashPassword(const std::string& password, const std::string& salt) {
    // Combine password and salt
    std::string combined = password + salt;
    
    // Create SHA-256 hash
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, combined.c_str(), combined.length());
    SHA256_Final(hash, &sha256);
    
    // Convert to hex string
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}

// Helper function to validate TOTP
bool validateTOTP(const std::string& secret, const std::string& code) {
    // Get current time
    auto now = std::chrono::system_clock::now();
    auto timeValue = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count() / 30;  // 30-second window
    
    // Try current and adjacent time windows
    for (int64_t offset = -1; offset <= 1; ++offset) {
        int64_t counter = timeValue + offset;
        
        // Generate HMAC-SHA1
        unsigned char hmac[EVP_MAX_MD_SIZE];
        unsigned int hmacLen;
        uint8_t counterBytes[8];
        for (int i = 7; i >= 0; --i) {
            counterBytes[i] = counter & 0xff;
            counter >>= 8;
        }
        
        HMAC(EVP_sha1(), secret.c_str(), secret.length(),
             counterBytes, sizeof(counterBytes),
             hmac, &hmacLen);
        
        // Get offset
        int offset = hmac[hmacLen - 1] & 0xf;
        
        // Generate 6-digit code
        int value = (hmac[offset] & 0x7f) << 24 |
                   (hmac[offset + 1] & 0xff) << 16 |
                   (hmac[offset + 2] & 0xff) << 8 |
                   (hmac[offset + 3] & 0xff);
        value = value % 1000000;
        
        // Compare with provided code
        std::stringstream ss;
        ss << std::setw(6) << std::setfill('0') << value;
        if (ss.str() == code) {
            return true;
        }
    }
    return false;
}

} // anonymous namespace

//-----------------------------------------------------------------------------
// AuthSystem Implementation
//-----------------------------------------------------------------------------

AuthSystem::AuthSystem() {
    // Initialize OpenSSL
    OpenSSL_add_all_algorithms();
}

bool AuthSystem::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // Create default system roles
        Role adminRole;
        adminRole.id = "admin";
        adminRole.name = "System Administrator";
        adminRole.description = "Full system access";
        adminRole.isSystem = true;
        adminRole.permissions = {
            Permission::SYSTEM_ADMIN,
            Permission::CONFIG_READ,
            Permission::CONFIG_WRITE,
            Permission::NODE_MANAGE,
            Permission::SIGNAL_DETECT,
            Permission::SIGNAL_ANALYZE,
            Permission::SIGNAL_CLASSIFY,
            Permission::SIGNAL_TRACK,
            Permission::DATA_READ,
            Permission::DATA_WRITE,
            Permission::DATA_DELETE,
            Permission::DATA_EXPORT,
            Permission::USER_MANAGE,
            Permission::ROLE_MANAGE,
            Permission::AUDIT_VIEW
        };
        roles_[adminRole.id] = adminRole;

        Role operatorRole;
        operatorRole.id = "operator";
        operatorRole.name = "System Operator";
        operatorRole.description = "Signal detection and analysis";
        operatorRole.isSystem = true;
        operatorRole.permissions = {
            Permission::CONFIG_READ,
            Permission::SIGNAL_DETECT,
            Permission::SIGNAL_ANALYZE,
            Permission::SIGNAL_CLASSIFY,
            Permission::SIGNAL_TRACK,
            Permission::DATA_READ,
            Permission::DATA_EXPORT
        };
        roles_[operatorRole.id] = operatorRole;

        Role analystRole;
        analystRole.id = "analyst";
        analystRole.name = "Signal Analyst";
        analystRole.description = "Signal analysis and reporting";
        analystRole.isSystem = true;
        analystRole.permissions = {
            Permission::SIGNAL_ANALYZE,
            Permission::SIGNAL_CLASSIFY,
            Permission::DATA_READ,
            Permission::DATA_EXPORT
        };
        roles_[analystRole.id] = analystRole;

        return true;
    }
    catch (const std::exception& e) {
        logAudit("system", "Failed to initialize auth system: " + std::string(e.what()));
        return false;
    }
}

AuthResult AuthSystem::authenticate(
    const std::string& username,
    const std::string& password,
    const std::map<AuthFactorType, std::string>& factors
) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    AuthResult result;
    result.success = false;
    
    try {
        // Find user
        auto userIt = std::find_if(users_.begin(), users_.end(),
            [&username](const auto& pair) {
                return pair.second.username == username;
            });
        
        if (userIt == users_.end()) {
            result.messages.push_back("Invalid username or password");
            logAudit("unknown", "Failed login attempt for username: " + username);
            return result;
        }
        
        const User& user = userIt->second;
        
        // Check if account is enabled and not locked
        if (!user.enabled) {
            result.messages.push_back("Account is disabled");
            logAudit(user.id, "Login attempt on disabled account");
            return result;
        }
        
        if (user.locked) {
            result.messages.push_back("Account is locked");
            logAudit(user.id, "Login attempt on locked account");
            return result;
        }
        
        // Validate password
        std::string salt = user.metadata.at("salt");
        std::string hashedPassword = hashPassword(password, salt);
        if (hashedPassword != user.passwordHash) {
            result.messages.push_back("Invalid username or password");
            logAudit(user.id, "Failed password validation");
            return result;
        }
        
        // Validate additional factors
        if (!validateFactors(user, factors)) {
            result.messages.push_back("Multi-factor authentication failed");
            logAudit(user.id, "Failed multi-factor authentication");
            return result;
        }
        
        // Generate session
        result.success = true;
        result.userId = user.id;
        result.sessionId = generateSessionId();
        result.permissions = getUserPermissions(user);
        result.messages.push_back("Authentication successful");
        
        // Update session storage
        sessions_[result.sessionId] = result;
        
        // Update user's last login time
        User& mutableUser = users_[user.id];
        mutableUser.lastLogin = std::chrono::system_clock::now();
        
        logAudit(user.id, "Successful login");
        
        // Notify callback if set
        if (authCallback_) {
            authCallback_(user.id, result);
        }
        
        return result;
    }
    catch (const std::exception& e) {
        result.messages.push_back("Authentication error: " + std::string(e.what()));
        logAudit("system", "Authentication error: " + std::string(e.what()));
        return result;
    }
}

bool AuthSystem::validateSession(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = sessions_.find(sessionId);
    if (it == sessions_.end()) {
        return false;
    }
    
    // TODO: Add session expiration check
    return true;
}

bool AuthSystem::checkPermission(const std::string& sessionId, Permission permission) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = sessions_.find(sessionId);
    if (it == sessions_.end()) {
        return false;
    }
    
    return it->second.permissions.find(permission) != it->second.permissions.end();
}

bool AuthSystem::createUser(const User& user) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // Validate user data
        if (user.username.empty() || user.email.empty() || user.passwordHash.empty()) {
            logAudit("system", "Failed to create user: Missing required fields");
            return false;
        }
        
        // Check for existing username
        auto existingUser = std::find_if(users_.begin(), users_.end(),
            [&user](const auto& pair) {
                return pair.second.username == user.username;
            });
        
        if (existingUser != users_.end()) {
            logAudit("system", "Failed to create user: Username already exists");
            return false;
        }
        
        // Store user
        users_[user.id] = user;
        
        logAudit("system", "Created user: " + user.id);
        return true;
    }
    catch (const std::exception& e) {
        logAudit("system", "Failed to create user: " + std::string(e.what()));
        return false;
    }
}

bool AuthSystem::updateUser(const User& user) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        auto it = users_.find(user.id);
        if (it == users_.end()) {
            logAudit("system", "Failed to update user: User not found");
            return false;
        }
        
        // Update user
        users_[user.id] = user;
        
        logAudit("system", "Updated user: " + user.id);
        return true;
    }
    catch (const std::exception& e) {
        logAudit("system", "Failed to update user: " + std::string(e.what()));
        return false;
    }
}

bool AuthSystem::deleteUser(const std::string& userId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        auto it = users_.find(userId);
        if (it == users_.end()) {
            logAudit("system", "Failed to delete user: User not found");
            return false;
        }
        
        // Remove all sessions for this user
        for (auto it = sessions_.begin(); it != sessions_.end();) {
            if (it->second.userId == userId) {
                it = sessions_.erase(it);
            } else {
                ++it;
            }
        }
        
        // Remove user
        users_.erase(userId);
        
        logAudit("system", "Deleted user: " + userId);
        return true;
    }
    catch (const std::exception& e) {
        logAudit("system", "Failed to delete user: " + std::string(e.what()));
        return false;
    }
}

bool AuthSystem::createRole(const Role& role) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // Validate role data
        if (role.id.empty() || role.name.empty()) {
            logAudit("system", "Failed to create role: Missing required fields");
            return false;
        }
        
        // Check for existing role
        if (roles_.find(role.id) != roles_.end()) {
            logAudit("system", "Failed to create role: Role ID already exists");
            return false;
        }
        
        // Store role
        roles_[role.id] = role;
        
        logAudit("system", "Created role: " + role.id);
        return true;
    }
    catch (const std::exception& e) {
        logAudit("system", "Failed to create role: " + std::string(e.what()));
        return false;
    }
}

bool AuthSystem::updateRole(const Role& role) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        auto it = roles_.find(role.id);
        if (it == roles_.end()) {
            logAudit("system", "Failed to update role: Role not found");
            return false;
        }
        
        // Prevent modification of system roles
        if (it->second.isSystem) {
            logAudit("system", "Failed to update role: Cannot modify system role");
            return false;
        }
        
        // Update role
        roles_[role.id] = role;
        
        logAudit("system", "Updated role: " + role.id);
        return true;
    }
    catch (const std::exception& e) {
        logAudit("system", "Failed to update role: " + std::string(e.what()));
        return false;
    }
}

bool AuthSystem::deleteRole(const std::string& roleId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        auto it = roles_.find(roleId);
        if (it == roles_.end()) {
            logAudit("system", "Failed to delete role: Role not found");
            return false;
        }
        
        // Prevent deletion of system roles
        if (it->second.isSystem) {
            logAudit("system", "Failed to delete role: Cannot delete system role");
            return false;
        }
        
        // Remove role from all users
        for (auto& [userId, user] : users_) {
            user.roleIds.erase(roleId);
        }
        
        // Remove role
        roles_.erase(roleId);
        
        logAudit("system", "Deleted role: " + roleId);
        return true;
    }
    catch (const std::exception& e) {
        logAudit("system", "Failed to delete role: " + std::string(e.what()));
        return false;
    }
}

void AuthSystem::setAuthCallback(AuthCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    authCallback_ = callback;
}

void AuthSystem::setAuditCallback(AuditCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    auditCallback_ = callback;
}

std::string AuthSystem::generateSessionId() const {
    // Generate 32 random bytes
    unsigned char bytes[32];
    if (RAND_bytes(bytes, sizeof(bytes)) != 1) {
        throw std::runtime_error("Failed to generate random bytes");
    }
    
    // Convert to hex string
    std::stringstream ss;
    for (int i = 0; i < sizeof(bytes); i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)bytes[i];
    }
    return ss.str();
}

bool AuthSystem::validateFactors(
    const User& user,
    const std::map<AuthFactorType, std::string>& factors) const {
    
    // Check each required factor
    for (const auto& factor : user.factors) {
        if (factor.required) {
            auto it = factors.find(factor.type);
            if (it == factors.end()) {
                return false;  // Required factor not provided
            }
            
            // Validate factor based on type
            switch (factor.type) {
                case AuthFactorType::TOTP:
                    if (!validateTOTP(factor.secret, it->second)) {
                        return false;
                    }
                    break;
                    
                case AuthFactorType::HARDWARE_TOKEN:
                    // TODO: Implement hardware token validation
                    break;
                    
                case AuthFactorType::CERTIFICATE:
                    // TODO: Implement certificate validation
                    break;
                    
                case AuthFactorType::BIOMETRIC:
                    // TODO: Implement biometric validation
                    break;
                    
                default:
                    break;
            }
        }
    }
    
    return true;
}

std::set<Permission> AuthSystem::getUserPermissions(const User& user) const {
    std::set<Permission> permissions;
    
    // Combine permissions from all roles
    for (const auto& roleId : user.roleIds) {
        auto it = roles_.find(roleId);
        if (it != roles_.end()) {
            const auto& rolePerms = it->second.permissions;
            permissions.insert(rolePerms.begin(), rolePerms.end());
        }
    }
    
    return permissions;
}

void AuthSystem::logAudit(const std::string& userId, const std::string& message) {
    if (auditCallback_) {
        auditCallback_(userId, message);
    }
}

} // namespace auth
} // namespace tdoa 