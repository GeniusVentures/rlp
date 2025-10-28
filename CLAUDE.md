# SuperGenius Development Guide

## Important Guidelines
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
- Always prefer const variables, const parameters, const functions. use const by default
- Keep right balance in programming between object and functional programming. Prefer functional in general with less state.
- Public methods should be as objects, but internal implementations can go more with functional programming patterns.
- Conform to Effective C++ book principles as close as possible
- Conform to Modern Effective C++ book principles as close as possible
- Conform to Effective STL book principles as close as possible
- Conform to API Design for C++ book principles as close as possible


## Testing Practice
- Unit tests should be placed in the test/ directory matching source structure
- Use cmake test framework for unit tests
- Test names should be descriptive of what they're testing