# SuperGenius Development Guide

## Important Guidelines
- NEVER modify files directly - suggest changes but let the user make the actual edits
- Do not commit changes without explicit user permission

## Build Commands
- Build project: `build.bat`
- Release build: `build.bat`
- Enable testing: Add `-DTESTING=ON` to cmake arguments

## Code Style
- Style: Based on Microsoft with modifications (see .clang-format)
- Indent: 4 spaces
- Line length: 120 characters maximum
- Classes/Methods: PascalCase
- Variables: camelCase
- Constants: ALL_CAPS
- Parentheses: space after opening and before closing: `if ( condition )`
- Braces: Each on their own line
- Error Handling: Use outcome::result<T> pattern for error propagation
- Namespaces: Use nested namespaces with full indentation
- Comments: Document interfaces and public methods
- Const-correctness: All parameter structs passed by const&
- Named initialization: Structs designed for designated initializers
- Grouped values: Related data in structs, returned by const&
- Meaningful constants: kPublicKeySize instead of magic 64
- Law of Demeter: Boost types hidden behind aliases
- Unique ownership: unique_ptr throughout, no shared_ptr
- Outcome-based errors: No exceptions in hot paths


## Testing Practice
- Unit tests should be placed in the test/ directory matching source structure
- Use cmake test framework for unit tests
- Test names should be descriptive of what they're testing