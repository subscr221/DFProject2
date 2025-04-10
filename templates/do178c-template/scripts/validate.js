#!/usr/bin/env node

const fs = require('fs');
const path = require('path');

// Required files and their validation rules
const REQUIRED_FILES = {
    'memory-bank/projectbrief.md': {
        required: true,
        sections: ['Project Overview', 'Organization', 'DO-178C Compliance Level']
    },
    'memory-bank/productContext.md': {
        required: true,
        sections: ['Project Purpose', 'Certification Context', 'Safety Impact Analysis']
    },
    'memory-bank/systemPatterns.md': {
        required: true,
        sections: ['System Architecture', 'Development Patterns', 'Verification Patterns']
    },
    'memory-bank/techContext.md': {
        required: true,
        sections: ['Development Environment', 'Tool Qualification', 'Technical Standards']
    },
    'memory-bank/activeContext.md': {
        required: true,
        sections: ['Current Certification Activities', 'Active Decisions', 'Next Steps']
    },
    'certification/planning/psac.md': {
        required: true,
        sections: ['Overview', 'Software Development Plan', 'Certification Strategy']
    }
};

// Metadata requirements
const REQUIRED_METADATA = [
    'Project Name',
    'Organization',
    'DO-178C Level',
    'Date'
];

function validateFile(filePath, rules) {
    console.log(`Validating ${filePath}...`);
    
    try {
        const content = fs.readFileSync(filePath, 'utf8');
        
        // Check required sections
        rules.sections.forEach(section => {
            if (!content.includes(`## ${section}`) && !content.includes(`### ${section}`)) {
                console.error(`  ERROR: Missing required section '${section}'`);
            }
        });
        
        // Check metadata
        if (filePath.endsWith('.md')) {
            validateMetadata(content);
        }
        
        // Check for traceability links
        validateTraceability(content);
        
        console.log(`  ✓ File validation complete`);
    } catch (error) {
        if (rules.required) {
            console.error(`  ERROR: Required file ${filePath} is missing or cannot be read`);
        } else {
            console.warn(`  WARNING: Optional file ${filePath} is missing or cannot be read`);
        }
    }
}

function validateMetadata(content) {
    const metadata = extractMetadata(content);
    REQUIRED_METADATA.forEach(field => {
        if (!metadata[field]) {
            console.warn(`  WARNING: Missing metadata field '${field}'`);
        }
    });
}

function extractMetadata(content) {
    const metadata = {};
    const lines = content.split('\n');
    let inMetadata = false;
    
    for (const line of lines) {
        if (line.trim() === '---') {
            inMetadata = !inMetadata;
            continue;
        }
        
        if (inMetadata) {
            const match = line.match(/^([^:]+):\s*(.+)$/);
            if (match) {
                metadata[match[1].trim()] = match[2].trim();
            }
        }
    }
    
    return metadata;
}

function validateTraceability(content) {
    // Check for requirement references
    const reqRefs = content.match(/REQ-\d+/g) || [];
    
    // Check for design references
    const designRefs = content.match(/DESIGN-\d+/g) || [];
    
    // Check for test references
    const testRefs = content.match(/TEST-\d+/g) || [];
    
    if (reqRefs.length === 0 && designRefs.length === 0 && testRefs.length === 0) {
        console.warn('  WARNING: No traceability references found');
    }
}

function validateDocumentationStructure() {
    console.log('\nValidating documentation structure...');
    
    // Check required directories
    ['memory-bank', 'certification', 'tasks', 'templates'].forEach(dir => {
        if (!fs.existsSync(dir)) {
            console.error(`ERROR: Required directory '${dir}' is missing`);
        }
    });
    
    // Validate each required file
    Object.entries(REQUIRED_FILES).forEach(([file, rules]) => {
        validateFile(file, rules);
    });
}

function validateTaskStructure() {
    console.log('\nValidating task structure...');
    
    try {
        const tasksJson = JSON.parse(fs.readFileSync('tasks/tasks.json', 'utf8'));
        
        // Check metadata
        if (!tasksJson.metadata) {
            console.error('ERROR: Missing metadata in tasks.json');
        }
        
        // Validate tasks
        if (!Array.isArray(tasksJson.tasks)) {
            console.error('ERROR: Invalid tasks structure in tasks.json');
        } else {
            tasksJson.tasks.forEach(task => {
                validateTask(task);
            });
        }
    } catch (error) {
        console.error('ERROR: Could not validate tasks.json:', error.message);
    }
}

function validateTask(task) {
    const requiredFields = ['id', 'title', 'description', 'status'];
    requiredFields.forEach(field => {
        if (!task[field]) {
            console.error(`ERROR: Task ${task.id || 'unknown'} missing required field '${field}'`);
        }
    });
}

function generateReport() {
    console.log('\nGenerating validation report...');
    
    // Add report generation logic here
    // - Summary of findings
    // - Compliance status
    // - Recommendations
}

function main() {
    console.log('DO-178C Documentation Validation\n');
    
    try {
        validateDocumentationStructure();
        validateTaskStructure();
        generateReport();
        
        console.log('\nValidation complete!');
    } catch (error) {
        console.error('\nError during validation:', error.message);
        process.exit(1);
    }
}

main(); 