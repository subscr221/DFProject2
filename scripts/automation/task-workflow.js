#!/usr/bin/env node

const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');
const yaml = require('js-yaml');

class TaskWorkflow {
    constructor(taskId, developer) {
        this.taskId = taskId;
        this.developer = developer;
        this.config = this.loadConfig();
        this.taskInfo = this.loadTaskInfo();
    }

    loadConfig() {
        const configPath = path.join(process.env.HOME, '.task-master/config.json');
        return JSON.parse(fs.readFileSync(configPath, 'utf8'));
    }

    loadTaskInfo() {
        const tasksPath = 'tasks/tasks.json';
        const tasks = JSON.parse(fs.readFileSync(tasksPath, 'utf8'));
        return tasks.tasks.find(t => t.id === this.taskId);
    }

    async startTask() {
        console.log(`Starting task ${this.taskId}...`);

        try {
            // 1. Create feature branch
            this.createFeatureBranch();

            // 2. Set up task environment
            await this.setupTaskEnvironment();

            // 3. Initialize documentation
            await this.initializeDocumentation();

            // 4. Set up testing framework
            await this.setupTesting();

            // 5. Configure CI/CD
            await this.configureCICD();

            console.log('Task setup complete!');
        } catch (error) {
            console.error('Error during task setup:', error);
            process.exit(1);
        }
    }

    createFeatureBranch() {
        const branchName = `feature/${this.developer}/${this.taskId}`;
        execSync('git checkout develop');
        execSync(`git checkout -b ${branchName}`);
        console.log(`Created branch: ${branchName}`);
    }

    async setupTaskEnvironment() {
        // 1. Create task directory
        const taskDir = `work/${this.taskId}`;
        fs.mkdirSync(taskDir, { recursive: true });

        // 2. Copy task template
        const templateType = this.getTemplateType();
        const templatePath = `templates/task-templates/${templateType}.md`;
        const taskTemplate = fs.readFileSync(templatePath, 'utf8');

        // 3. Fill template with task info
        const filledTemplate = this.fillTemplate(taskTemplate);
        fs.writeFileSync(`${taskDir}/README.md`, filledTemplate);

        // 4. Initialize development environment
        await this.initializeDevEnvironment(taskDir);
    }

    getTemplateType() {
        const devConfig = this.config.taskDistribution.rules[this.developer];
        return `${devConfig.prefix.toLowerCase()}-template`;
    }

    fillTemplate(template) {
        // Replace template variables with task info
        return template.replace(/\${(\w+)}/g, (match, key) => {
            return this.taskInfo[key] || '';
        });
    }

    async initializeDocumentation() {
        const docsDir = `docs/${this.taskId}`;
        fs.mkdirSync(docsDir, { recursive: true });

        // Create required DO-178C documentation
        const docTypes = [
            'requirements',
            'design',
            'test-plan',
            'verification',
            'validation'
        ];

        for (const type of docTypes) {
            const docPath = `${docsDir}/${type}.md`;
            const template = fs.readFileSync(`templates/docs/${type}-template.md`, 'utf8');
            const filledTemplate = this.fillTemplate(template);
            fs.writeFileSync(docPath, filledTemplate);
        }
    }

    async setupTesting() {
        const testDir = `tests/${this.taskId}`;
        fs.mkdirSync(testDir, { recursive: true });

        // Create test structure
        const testTypes = [
            'unit',
            'integration',
            'performance',
            'compliance'
        ];

        for (const type of testTypes) {
            const testPath = `${testDir}/${type}`;
            fs.mkdirSync(testPath, { recursive: true });
            
            // Copy test templates
            const templatePath = `templates/tests/${type}-template`;
            if (fs.existsSync(templatePath)) {
                execSync(`cp -r ${templatePath}/* ${testPath}/`);
            }
        }
    }

    async configureCICD() {
        // Create CI/CD configuration
        const ciConfig = {
            task_id: this.taskId,
            developer: this.developer,
            checks: [
                'do178c-compliance',
                'unit-tests',
                'integration-tests',
                'performance-tests',
                'documentation-validation'
            ],
            requirements: {
                coverage: 90,
                performance: {
                    latency: '${this.taskInfo.maxLatency}',
                    throughput: '${this.taskInfo.targetThroughput}'
                },
                compliance: {
                    level: 'C',
                    requirements: ['DO-178C', 'DO-330']
                }
            }
        };

        fs.writeFileSync(`.github/workflows/${this.taskId}.yml`, yaml.dump(ciConfig));
    }

    async completeTask() {
        console.log(`Completing task ${this.taskId}...`);

        try {
            // 1. Run compliance checks
            await this.runComplianceChecks();

            // 2. Update documentation
            await this.updateDocumentation();

            // 3. Create pull request
            await this.createPullRequest();

            console.log('Task completion process finished!');
        } catch (error) {
            console.error('Error during task completion:', error);
            process.exit(1);
        }
    }

    async runComplianceChecks() {
        console.log('Running compliance checks...');
        
        // Run all verification steps
        execSync('npm run validate');
        execSync('npm run test');
        execSync('npm run lint');
        execSync('npm run coverage');

        // Validate DO-178C compliance
        execSync('task-master validate-compliance');
    }

    async updateDocumentation() {
        console.log('Updating documentation...');
        
        // Generate documentation updates
        execSync('npm run docs');

        // Update traceability matrix
        execSync('task-master update-traceability');

        // Validate documentation
        execSync('task-master validate-docs');
    }

    async createPullRequest() {
        console.log('Creating pull request...');

        // Get reviewers from config
        const devConfig = this.config.taskDistribution.rules[this.developer];
        const reviewers = devConfig.reviewers.join(',');

        // Create PR
        execSync(`gh pr create --base develop --reviewer ${reviewers} --title "${this.taskId}: ${this.taskInfo.title}" --body "$(cat .github/PULL_REQUEST_TEMPLATE.md)"`);
    }
}

// CLI interface
const args = process.argv.slice(2);
const [command, taskId, developer] = args;

const workflow = new TaskWorkflow(taskId, developer);

switch (command) {
    case 'start':
        workflow.startTask();
        break;
    case 'complete':
        workflow.completeTask();
        break;
    default:
        console.error('Unknown command. Use "start" or "complete"');
        process.exit(1);
} 