# GitHub Actions Workflows

## Active Workflows

### ci-cd.yml
Main CI/CD pipeline providing:
- Multi-architecture builds (i686, x86_64)
- Comprehensive static analysis
- Regression testing
- Code quality checks
- Security scanning
- Integration testing
- Performance monitoring
- Artifact management

### release.yml
Release workflow for creating distribution packages.

### generate-kernel-feature-issue.yml
Automation for kernel feature issue tracking.

## Previous Consolidation

The original separate workflow files (build-test.yml, build-test-i686.yml, build-test-x86_64.yml, ci.yml) were consolidated into the main `ci-cd.yml` pipeline for improved efficiency and reduced duplication.