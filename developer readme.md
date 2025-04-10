# Clone the repositories
git clone git@github.com:organization/tdoa-direction-finder.git
git clone git@github.com:organization/tdoa-docs.git
git clone git@github.com:organization/tdoa-verification.git

# Install task-master globally
npm install -g task-master

# Setup local configuration
cp task-master.config.json ~/.task-master/config.json

# Initialize their workspace
task-master init --developer=dev<N>

# Get assigned tasks
task-master list --assignee=dev<N>

# Start working on a task
task-master start --id=<task-id>

# Create feature branch
task-master create-branch --id=<task-id>

# Update task status
task-master update --id=<task-id> --status="in-progress"

# Request review
task-master review --id=<task-id>

# Complete task
task-master complete --id=<task-id>

# List pending reviews
task-master reviews --pending

# Review a task
task-master review --id=<task-id> --action=approve|reject

# Check compliance
task-master validate --id=<task-id>

# Prepare for integration
task-master integrate --id=<task-id>

# Run compliance checks
task-master validate-compliance

# Update documentation
task-master update-docs

# Complete integration
task-master merge --id=<task-id>
