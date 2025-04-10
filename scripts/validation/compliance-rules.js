#!/usr/bin/env node

const fs = require('fs');
const path = require('path');

class ComplianceValidator {
    constructor(taskId, developer) {
        this.taskId = taskId;
        this.developer = developer;
        this.config = this.loadConfig();
    }

    loadConfig() {
        return JSON.parse(fs.readFileSync('.task-master/config.json', 'utf8'));
    }

    async validateCompliance() {
        console.log('Validating DO-178C compliance...');

        try {
            // 1. Documentation compliance
            await this.validateDocumentation();

            // 2. Code compliance
            await this.validateCode();

            // 3. Test compliance
            await this.validateTests();

            // 4. Tool qualification
            await this.validateTools();

            // 5. Process compliance
            await this.validateProcess();

            console.log('Compliance validation complete!');
            return true;
        } catch (error) {
            console.error('Compliance validation failed:', error);
            return false;
        }
    }

    async validateDocumentation() {
        console.log('Checking documentation compliance...');

        const requiredDocs = {
            'requirements.md': {
                sections: ['Overview', 'Requirements', 'Safety', 'Traceability'],
                metadata: ['Level', 'Status', 'Review']
            },
            'design.md': {
                sections: ['Architecture', 'Components', 'Interfaces', 'Safety'],
                metadata: ['Level', 'Status', 'Review']
            },
            'test-plan.md': {
                sections: ['Strategy', 'Cases', 'Coverage', 'Validation'],
                metadata: ['Level', 'Status', 'Review']
            },
            'verification.md': {
                sections: ['Methods', 'Results', 'Analysis', 'Conclusion'],
                metadata: ['Level', 'Status', 'Review']
            }
        };

        for (const [doc, rules] of Object.entries(requiredDocs)) {
            const docPath = path.join('docs', this.taskId, doc);
            if (!fs.existsSync(docPath)) {
                throw new Error(`Missing required document: ${doc}`);
            }

            const content = fs.readFileSync(docPath, 'utf8');
            this.validateDocumentContent(content, rules);
        }
    }

    validateDocumentContent(content, rules) {
        // Check required sections
        rules.sections.forEach(section => {
            if (!content.includes(`## ${section}`)) {
                throw new Error(`Missing required section: ${section}`);
            }
        });

        // Check metadata
        const metadata = this.extractMetadata(content);
        rules.metadata.forEach(field => {
            if (!metadata[field]) {
                throw new Error(`Missing required metadata: ${field}`);
            }
        });

        // Check traceability
        this.validateTraceability(content);
    }

    async validateCode() {
        console.log('Checking code compliance...');

        const rules = {
            complexity: {
                maxCyclomaticComplexity: 10,
                maxNestingDepth: 4,
                maxFunctionLength: 50
            },
            safety: {
                requireErrorHandling: true,
                requireBoundsChecking: true,
                requireNullChecking: true,
                requireTypeChecking: true
            },
            style: {
                requireComments: true,
                requireJSDoc: true,
                enforceNaming: true
            }
        };

        // Run static analysis
        await this.runStaticAnalysis(rules);

        // Check code coverage
        await this.checkCodeCoverage();

        // Validate safety patterns
        await this.validateSafetyPatterns();
    }

    async validateTests() {
        console.log('Checking test compliance...');

        const requirements = {
            coverage: {
                statements: 90,
                branches: 90,
                functions: 100,
                lines: 90
            },
            types: {
                unit: true,
                integration: true,
                system: true,
                performance: true
            },
            documentation: {
                requireTestPlan: true,
                requireTestCases: true,
                requireResults: true,
                requireAnalysis: true
            }
        };

        // Validate test coverage
        await this.validateTestCoverage(requirements.coverage);

        // Check test types
        await this.validateTestTypes(requirements.types);

        // Verify test documentation
        await this.validateTestDocumentation(requirements.documentation);
    }

    async validateTools() {
        console.log('Checking tool qualification...');

        const toolRequirements = {
            development: {
                requireQualification: true,
                requireVersionControl: true,
                requireConfiguration: true
            },
            verification: {
                requireQualification: true,
                requireCalibration: true,
                requireValidation: true
            },
            testing: {
                requireQualification: true,
                requireAutomation: true,
                requireReporting: true
            }
        };

        // Validate development tools
        await this.validateDevelopmentTools(toolRequirements.development);

        // Check verification tools
        await this.validateVerificationTools(toolRequirements.verification);

        // Verify testing tools
        await this.validateTestingTools(toolRequirements.testing);
    }

    async validateProcess() {
        console.log('Checking process compliance...');

        const processRequirements = {
            planning: {
                requirePlan: true,
                requireSchedule: true,
                requireResources: true
            },
            development: {
                requireMethodology: true,
                requireStandards: true,
                requireReviews: true
            },
            verification: {
                requireStrategy: true,
                requireIndependence: true,
                requireTraceability: true
            },
            configuration: {
                requireVersionControl: true,
                requireChangeControl: true,
                requireBaseline: true
            }
        };

        // Validate planning process
        await this.validatePlanningProcess(processRequirements.planning);

        // Check development process
        await this.validateDevelopmentProcess(processRequirements.development);

        // Verify verification process
        await this.validateVerificationProcess(processRequirements.verification);

        // Check configuration management
        await this.validateConfigurationProcess(processRequirements.configuration);
    }
}

// CLI interface
const args = process.argv.slice(2);
const [taskId, developer] = args;

const validator = new ComplianceValidator(taskId, developer);
validator.validateCompliance()
    .then(success => process.exit(success ? 0 : 1))
    .catch(error => {
        console.error('Validation failed:', error);
        process.exit(1);
    }); 