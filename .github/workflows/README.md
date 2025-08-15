# Deprecated Workflows

The following workflow files have been consolidated into the main `ci-cd.yml` pipeline:

- `build-test.yml.deprecated` - Original matrix build workflow
- `build-test-i686.yml.deprecated` - i686-specific build workflow  
- `build-test-x86_64.yml.deprecated` - x86_64-specific build workflow
- `ci.yml.deprecated` - Original CI workflow

## Migration to Consolidated CI/CD Pipeline

All functionality from the deprecated workflows has been integrated into `ci-cd.yml` which provides:

- Multi-architecture builds (i686, x86_64)
- Comprehensive static analysis
- Regression testing
- Code quality checks
- Security scanning
- Integration testing
- Performance monitoring
- Better artifact management

The consolidated workflow is more efficient, reduces duplication, and provides better visibility into the build and test process.

## Cleanup

These deprecated files can be safely removed after confirming the new CI/CD pipeline works correctly.