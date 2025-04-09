#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <set>
#include <chrono>
#include <functional>
#include <mutex>

namespace tdoa {
namespace auth {

/**
 * @enum AuthFactorType
 * @brief Types of authentication factors supported
 */
enum class AuthFactorType {
    PASSWORD,       ///< Password-based authentication
    TOTP,          ///< Time-based one-time password
    HARDWARE_TOKEN, ///< Hardware security token
    CERTIFICATE,   ///< Client certificate
    BIOMETRIC      ///< Biometric authentication
};

/**
 * @struct AuthFactor
 * @brief Represents an authentication factor configuration
 */
struct AuthFactor {
    AuthFactorType type;           ///< Type of authentication factor
    std::string identifier;        ///< Unique identifier for this factor
    std::string secret;           ///< Encrypted secret/configuration
    bool required;                ///< Whether this factor is required
    std::chrono::system_clock::time_point lastUsed; ///< Last successful use
};

/**
 * @enum Permission
 * @brief System permissions that can be granted to roles
 */
enum class Permission {
    // System Management
    SYSTEM_ADMIN,           ///< Full system administration
    CONFIG_READ,            ///< Read system configuration
    CONFIG_WRITE,           ///< Modify system configuration
    NODE_MANAGE,            ///< Manage system nodes
    
    // Signal Processing
    SIGNAL_DETECT,          ///< Access signal detection
    SIGNAL_ANALYZE,         ///< Analyze detected signals
    SIGNAL_CLASSIFY,        ///< Classify signals
    SIGNAL_TRACK,           ///< Track signals over time
    
    // Data Management
    DATA_READ,              ///< Read signal data
    DATA_WRITE,            ///< Write/modify signal data
    DATA_DELETE,           ///< Delete signal data
    DATA_EXPORT,           ///< Export signal data
    
    // User Management
    USER_MANAGE,           ///< Manage user accounts
    ROLE_MANAGE,           ///< Manage roles and permissions
    AUDIT_VIEW             ///< View audit logs
};

/**
 * @struct Role
 * @brief Defines a role with associated permissions
 */
struct Role {
    std::string id;                        ///< Unique role identifier
    std::string name;                      ///< Human-readable role name
    std::string description;               ///< Role description
    std::set<Permission> permissions;       ///< Granted permissions
    bool isSystem;                         ///< Whether this is a system role
};

/**
 * @struct User
 * @brief Represents a system user
 */
struct User {
    std::string id;                        ///< Unique user identifier
    std::string username;                  ///< Username for login
    std::string email;                     ///< User email address
    std::string passwordHash;              ///< Hashed password
    std::vector<AuthFactor> factors;       ///< Authentication factors
    std::set<std::string> roleIds;         ///< Assigned role IDs
    bool enabled;                          ///< Whether account is enabled
    bool locked;                           ///< Whether account is locked
    std::chrono::system_clock::time_point lastLogin; ///< Last successful login
    std::chrono::system_clock::time_point created;   ///< Account creation time
    std::map<std::string, std::string> metadata;     ///< Additional metadata
};

/**
 * @struct AuthResult
 * @brief Result of an authentication attempt
 */
struct AuthResult {
    bool success;                          ///< Whether authentication succeeded
    std::string userId;                    ///< ID of authenticated user
    std::string sessionId;                 ///< Session ID if successful
    std::vector<std::string> messages;     ///< Info/error messages
    std::set<Permission> permissions;       ///< Granted permissions
};

/**
 * @class AuthSystem
 * @brief Core authentication and authorization system
 */
class AuthSystem {
public:
    using AuthCallback = std::function<void(const std::string&, const AuthResult&)>;
    using AuditCallback = std::function<void(const std::string&, const std::string&)>;

    /**
     * @brief Constructor
     */
    AuthSystem();

    /**
     * @brief Initialize the authentication system
     * @return True if initialization successful
     */
    bool initialize();

    /**
     * @brief Authenticate a user
     * @param username Username
     * @param password Password
     * @param factors Additional authentication factors
     * @return Authentication result
     */
    AuthResult authenticate(
        const std::string& username,
        const std::string& password,
        const std::map<AuthFactorType, std::string>& factors = {}
    );

    /**
     * @brief Validate a session
     * @param sessionId Session ID to validate
     * @return True if session is valid
     */
    bool validateSession(const std::string& sessionId);

    /**
     * @brief Check if a session has a specific permission
     * @param sessionId Session ID to check
     * @param permission Permission to verify
     * @return True if session has permission
     */
    bool checkPermission(const std::string& sessionId, Permission permission);

    /**
     * @brief Create a new user
     * @param user User to create
     * @return True if user created successfully
     */
    bool createUser(const User& user);

    /**
     * @brief Update an existing user
     * @param user User to update
     * @return True if user updated successfully
     */
    bool updateUser(const User& user);

    /**
     * @brief Delete a user
     * @param userId ID of user to delete
     * @return True if user deleted successfully
     */
    bool deleteUser(const std::string& userId);

    /**
     * @brief Create a new role
     * @param role Role to create
     * @return True if role created successfully
     */
    bool createRole(const Role& role);

    /**
     * @brief Update an existing role
     * @param role Role to update
     * @return True if role updated successfully
     */
    bool updateRole(const Role& role);

    /**
     * @brief Delete a role
     * @param roleId ID of role to delete
     * @return True if role deleted successfully
     */
    bool deleteRole(const std::string& roleId);

    /**
     * @brief Set callback for authentication events
     * @param callback Function to call on auth events
     */
    void setAuthCallback(AuthCallback callback);

    /**
     * @brief Set callback for audit events
     * @param callback Function to call on audit events
     */
    void setAuditCallback(AuditCallback callback);

private:
    /**
     * @brief Generate a new session ID
     * @return Unique session ID
     */
    std::string generateSessionId() const;

    /**
     * @brief Validate authentication factors
     * @param user User to validate factors for
     * @param factors Provided factor values
     * @return True if factors are valid
     */
    bool validateFactors(const User& user,
                        const std::map<AuthFactorType, std::string>& factors) const;

    /**
     * @brief Get all permissions for a user
     * @param user User to get permissions for
     * @return Set of granted permissions
     */
    std::set<Permission> getUserPermissions(const User& user) const;

    /**
     * @brief Log an audit event
     * @param userId User ID associated with event
     * @param message Event message
     */
    void logAudit(const std::string& userId, const std::string& message);

private:
    std::map<std::string, User> users_;
    std::map<std::string, Role> roles_;
    std::map<std::string, AuthResult> sessions_;
    AuthCallback authCallback_;
    AuditCallback auditCallback_;
    mutable std::mutex mutex_;
};

} // namespace auth
} // namespace tdoa 