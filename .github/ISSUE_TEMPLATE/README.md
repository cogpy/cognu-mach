# Kernel Feature Issue Template

This directory contains the "Kernel Feature Issue" template and automation for generating structured issues for kernel feature implementation.

## Files

- `kernel_feature.yaml` - GitHub issue template for manual kernel feature issue creation
- `../workflows/generate-kernel-feature-issue.yml` - GitHub Action workflow for automated issue generation
- `config.yml` - Issue template configuration (updated to include automated workflow link)

## Usage

### Manual Issue Creation

1. Go to the [New Issue page](https://github.com/Kaw-Ai/cognu-mach/issues/new/choose)
2. Select "Kernel Feature Issue" template
3. Fill in all required fields
4. Submit the issue

### Automated Issue Generation

1. Go to [Actions > Generate Kernel Feature Issue](https://github.com/Kaw-Ai/cognu-mach/actions/workflows/generate-kernel-feature-issue.yml)
2. Click "Run workflow"
3. Fill in the workflow parameters:
   - **Feature Name**: Name of the kernel feature (required)
   - **Context**: Component area or context (required)
   - **Category**: Select from dropdown (required)
   - **Priority**: Select priority level (required)  
   - **Complexity**: Select complexity level (required)
   - **Estimated Effort**: Time estimation (required)
   - **Tasks**: JSON array of implementation tasks (required)
   - **Criteria**: JSON array of acceptance criteria (required)
   - **Epic**: Parent project (optional, defaults to "Kernel Modernization")
   - Other fields have sensible defaults but can be customized

### JSON Array Format for Lists

For fields that accept JSON arrays (tasks, criteria, etc.), use this format:
```json
["First item", "Second item", "Third item"]
```

Example for tasks:
```json
["Define kernel data structures", "Implement core algorithms", "Add system call interface", "Update documentation"]
```

## Template Structure

The generated issues follow this structure:

```
## [Feature Name]

**Category:** [Category] | **Priority:** [Priority] | **Complexity:** [Complexity]
**Estimated Effort:** [Duration]

### Description
Implement [feature] for [context]

### Implementation Requirements
- [ ] [Task 1]
- [ ] [Task 2]
...

### Technical Specifications
**Performance Targets:**
- [Target 1]
- [Target 2]
...

**Supported Devices:**
- [Device 1]
- [Device 2]
...

### Code Structure
**Files to implement:**
- [File 1]
- [File 2]
...

**Key Functions:**
- [Function 1]
- [Function 2]
...

### Testing Requirements
- [Test 1]
- [Test 2]
...

### Dependencies
- [Dependency 1]
- [Dependency 2]
...

### Files to Create
- [ ] [File 1]
- [ ] [File 2]
...

### Implementation Notes
This feature is part of the [Epic] implementation.

See the [Epic implementation specification](https://github.com/[path]) for architectural context.

**[Standards] Compliance:** All implementations must follow [Standards]

### Acceptance Criteria
- [ ] [Criterion 1]
- [ ] [Criterion 2]
...

*Feature ID: [Generated ID] | Auto-generated from [Epic] implementation workflow*
```

## Features

- **Automatic Feature ID generation**: Format `KERN-YYYYMMDD-[feature-name]`
- **Structured formatting**: Consistent issue format with checkboxes for tasks
- **Flexible parameterization**: Support for various kernel feature types
- **Integration with existing templates**: Works alongside phase-based templates
- **Validation**: YAML validation ensures proper template structure