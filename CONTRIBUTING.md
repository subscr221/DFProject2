# Contributing to TDOA Direction Finder System

## Development Guidelines

### Project Structure
```
/
├── README.md                 # Project overview
├── CONTRIBUTING.md          # Contribution guidelines
├── requirements.txt         # Python dependencies
├── CMakeLists.txt          # C++ build configuration
├── .gitignore              # Git ignore patterns
├── src/                    # Source code
│   └── README.md          # Source code documentation
├── lib/                    # External libraries
│   └── README.md          # Library documentation
├── include/               # Header files
│   └── README.md          # API documentation
├── test/                  # Test files
│   ├── README.md          # Test documentation
│   ├── unit/              # Unit tests
│   └── integration/       # Integration tests
├── temp/                  # Temporary files
│   └── README.md          # Usage guidelines
├── docs/                  # Documentation
│   └── README.md          # Documentation guide
└── scripts/               # Utility scripts
    └── README.md          # Script documentation
```

### Development Strategy
1. **Modular Development**
   - Implement and test features independently
   - Follow object-oriented design principles
   - Create reusable library components
   - Document module interfaces clearly
   - Write comprehensive unit tests

2. **Version Control Guidelines**
   - Create feature branches for each module
   - Make regular, descriptive commits
   - Tag stable releases
   - Protect the main branch
   - Integrate with CI/CD pipeline

3. **Git Workflow**
   ```
   main              # Stable production code
   ├── develop       # Integration branch
   ├── feature/*     # Feature branches
   ├── bugfix/*      # Bug fix branches
   └── release/*     # Release branches
   ```

4. **Commit Message Format**
   ```
   <type>(<scope>): <description>

   [optional body]

   [optional footer]
   ```
   Types: feat, fix, docs, style, refactor, test, chore

### Testing Requirements
1. **Unit Testing**
   - Write tests before implementing features
   - Maintain >80% code coverage
   - Test all public interfaces
   - Document test cases

2. **Integration Testing**
   - Test module interactions
   - Verify system-level functionality
   - Benchmark performance
   - Test error handling

### Documentation Standards
1. **Code Documentation**
   - Document all public APIs
   - Include usage examples
   - Explain complex algorithms
   - Update docs with code changes

2. **README Files**
   - Each directory must have a README
   - Explain directory purpose
   - List key files/modules
   - Provide usage instructions

### Development Environment
1. **Required Tools**
   - C++ compiler (GCC/Clang)
   - Python 3.8+
   - CMake 3.15+
   - CUDA Toolkit 11.0+
   - Git 2.25+

2. **IDE Configuration**
   - Use provided .vscode settings
   - Follow code style guide
   - Use recommended extensions
   - Configure debug settings 