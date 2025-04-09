#include "../auth_system.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <thread>

using namespace tdoa::auth;
using ::testing::_;
using ::testing::Return;

class MockAuthCallback {
public:
    MOCK_METHOD(void, onAuth, (const std::string&, const AuthResult&));
};

class MockAuditCallback {
public:
    MOCK_METHOD(void, onAudit, (const std::string&, const std::string&));
};

class AuthSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        authSystem.initialize();
        
        // Set up mock callbacks
        authSystem.setAuthCallback([this](const std::string& userId, const AuthResult& result) {
            mockAuthCallback.onAuth(userId, result);
        });
        
        authSystem.setAuditCallback([this](const std::string& userId, const std::string& message) {
            mockAuditCallback.onAudit(userId, message);
        });
        
        // Create test user
        User testUser;
        testUser.id = "test_user";
        testUser.username = "testuser";
        testUser.email = "test@example.com";
        testUser.enabled = true;
        testUser.locked = false;
        testUser.roleIds = {"operator"};
        
        // Generate salt and hash password
        std::string salt = "testsalt";
        testUser.metadata["salt"] = salt;
        testUser.passwordHash = hashPassword("testpass", salt);
        
        // Add TOTP factor
        AuthFactor totpFactor;
        totpFactor.type = AuthFactorType::TOTP;
        totpFactor.secret = "TESTSECRET";
        totpFactor.required = true;
        testUser.factors.push_back(totpFactor);
        
        authSystem.createUser(testUser);
    }
    
    std::string hashPassword(const std::string& password, const std::string& salt) {
        unsigned char hash[SHA256_DIGEST_LENGTH];
        std::string combined = password + salt;
        
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        SHA256_Update(&sha256, combined.c_str(), combined.length());
        SHA256_Final(hash, &sha256);
        
        std::stringstream ss;
        for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
        }
        return ss.str();
    }
    
    AuthSystem authSystem;
    MockAuthCallback mockAuthCallback;
    MockAuditCallback mockAuditCallback;
};

TEST_F(AuthSystemTest, InitializeCreatesSystemRoles) {
    // Verify system roles were created during initialization
    EXPECT_TRUE(authSystem.checkPermission("admin", Permission::SYSTEM_ADMIN));
    EXPECT_TRUE(authSystem.checkPermission("operator", Permission::SIGNAL_DETECT));
    EXPECT_TRUE(authSystem.checkPermission("analyst", Permission::SIGNAL_ANALYZE));
}

TEST_F(AuthSystemTest, AuthenticateSuccess) {
    // Set up mock expectations
    EXPECT_CALL(mockAuthCallback, onAuth(_, _))
        .Times(1);
    EXPECT_CALL(mockAuditCallback, onAudit("test_user", "Successful login"))
        .Times(1);
    
    // Prepare authentication factors
    std::map<AuthFactorType, std::string> factors;
    factors[AuthFactorType::TOTP] = "123456";  // Mock TOTP code
    
    // Attempt authentication
    AuthResult result = authSystem.authenticate("testuser", "testpass", factors);
    
    // Verify result
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.userId, "test_user");
    EXPECT_FALSE(result.sessionId.empty());
    EXPECT_TRUE(result.permissions.find(Permission::SIGNAL_DETECT) != result.permissions.end());
}

TEST_F(AuthSystemTest, AuthenticateFailInvalidPassword) {
    // Set up mock expectations
    EXPECT_CALL(mockAuditCallback, onAudit("test_user", "Failed password validation"))
        .Times(1);
    
    // Attempt authentication with wrong password
    std::map<AuthFactorType, std::string> factors;
    factors[AuthFactorType::TOTP] = "123456";
    
    AuthResult result = authSystem.authenticate("testuser", "wrongpass", factors);
    
    // Verify result
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.messages.front().find("Invalid username or password") != std::string::npos);
}

TEST_F(AuthSystemTest, AuthenticateFailMissingFactor) {
    // Set up mock expectations
    EXPECT_CALL(mockAuditCallback, onAudit("test_user", "Failed multi-factor authentication"))
        .Times(1);
    
    // Attempt authentication without required factor
    std::map<AuthFactorType, std::string> factors;
    
    AuthResult result = authSystem.authenticate("testuser", "testpass", factors);
    
    // Verify result
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.messages.front().find("Multi-factor authentication failed") != std::string::npos);
}

TEST_F(AuthSystemTest, SessionValidation) {
    // Authenticate to get a session
    std::map<AuthFactorType, std::string> factors;
    factors[AuthFactorType::TOTP] = "123456";
    
    AuthResult result = authSystem.authenticate("testuser", "testpass", factors);
    EXPECT_TRUE(result.success);
    
    // Verify session is valid
    EXPECT_TRUE(authSystem.validateSession(result.sessionId));
    
    // Verify invalid session is rejected
    EXPECT_FALSE(authSystem.validateSession("invalid_session"));
}

TEST_F(AuthSystemTest, PermissionChecking) {
    // Authenticate to get a session
    std::map<AuthFactorType, std::string> factors;
    factors[AuthFactorType::TOTP] = "123456";
    
    AuthResult result = authSystem.authenticate("testuser", "testpass", factors);
    EXPECT_TRUE(result.success);
    
    // Check permissions
    EXPECT_TRUE(authSystem.checkPermission(result.sessionId, Permission::SIGNAL_DETECT));
    EXPECT_FALSE(authSystem.checkPermission(result.sessionId, Permission::SYSTEM_ADMIN));
}

TEST_F(AuthSystemTest, UserManagement) {
    // Create new user
    User newUser;
    newUser.id = "new_user";
    newUser.username = "newuser";
    newUser.email = "new@example.com";
    newUser.enabled = true;
    newUser.locked = false;
    newUser.roleIds = {"analyst"};
    newUser.metadata["salt"] = "newsalt";
    newUser.passwordHash = hashPassword("newpass", "newsalt");
    
    EXPECT_TRUE(authSystem.createUser(newUser));
    
    // Update user
    newUser.email = "updated@example.com";
    EXPECT_TRUE(authSystem.updateUser(newUser));
    
    // Delete user
    EXPECT_TRUE(authSystem.deleteUser(newUser.id));
}

TEST_F(AuthSystemTest, RoleManagement) {
    // Create new role
    Role newRole;
    newRole.id = "custom_role";
    newRole.name = "Custom Role";
    newRole.description = "Custom role for testing";
    newRole.isSystem = false;
    newRole.permissions = {Permission::DATA_READ, Permission::DATA_EXPORT};
    
    EXPECT_TRUE(authSystem.createRole(newRole));
    
    // Update role
    newRole.permissions.insert(Permission::DATA_WRITE);
    EXPECT_TRUE(authSystem.updateRole(newRole));
    
    // Attempt to modify system role (should fail)
    Role adminRole;
    adminRole.id = "admin";
    adminRole.name = "Modified Admin";
    EXPECT_FALSE(authSystem.updateRole(adminRole));
    
    // Delete role
    EXPECT_TRUE(authSystem.deleteRole(newRole.id));
    
    // Attempt to delete system role (should fail)
    EXPECT_FALSE(authSystem.deleteRole("admin"));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 