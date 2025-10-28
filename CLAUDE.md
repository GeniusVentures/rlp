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
- If possible do not use inout parameters. Only in const and return results. If needed wrapped in custom structures.
- Keep right balance in programming between object and functional programming. Prefer functional in general with less state.
- Public methods should conform to object oriented deisgn, but internal implementations can go more with functional programming patterns.
- Conform to Effective C++ book principles as close as possible
- Conform to Modern Effective C++ book principles as close as possible
- Conform to Effective STL book principles as close as possible
- Conform to API Design for C++ book principles as close as possible
- Prefer to use coroutines for high latency operations like disk io, network io, gpu work or others

## C++ Coding Rules (Based on Effective C++)

### Language Fundamentals
- Adapt your programming style based on the C++ sublanguage you're using (C, Object-Oriented C++, Template C++, STL)
- Replace #define constants with const objects or enums
- Replace function-like macros with inline functions
- Use const everywhere possible: objects, parameters, return types, and member functions
- Always initialize objects before use; prefer member initialization lists over assignments in constructor bodies
- List data members in initialization lists in the same order they're declared in the class

### Constructors, Destructors, and Assignment
- Be aware of compiler-generated special member functions (default constructor, copy constructor, copy assignment, destructor)
- Explicitly delete or declare private any compiler-generated functions you don't want
- Always declare destructors virtual in polymorphic base classes
- If a class has any virtual functions, it must have a virtual destructor
- Never allow exceptions to escape from destructors; catch and handle them internally
- Never call virtual functions during construction or destruction
- Have assignment operators return a reference to *this
- Handle self-assignment in operator= using address comparison, careful statement ordering, or copy-and-swap
- When writing copy constructors or copy assignment operators, copy all data members and all base class parts

### Resource Management
- Use RAII (Resource Acquisition Is Initialization) - acquire resources in constructors, release in destructors
- Use smart pointers (unique_ptr preferred) to manage dynamically allocated resources
- Provide explicit conversion functions to access raw resources when needed for legacy APIs
- Always match array new[] with array delete[], and scalar new with scalar delete
- Store newed objects in smart pointers in standalone statements to prevent exception-related resource leaks
- Think carefully about copying behavior for resource-managing classes (disable, reference count, or deep copy)

### Interface Design and Declarations
- Design interfaces to be easy to use correctly and hard to use incorrectly
- Use strong types, restrict operations, constrain values, and eliminate client resource management responsibilities
- Prefer pass-by-reference-to-const over pass-by-value for efficiency and to avoid slicing
- Only pass built-in types and STL iterators/function objects by value
- Never return references or pointers to local stack objects
- Never return references to heap-allocated objects that could cause memory leaks
- Declare all data members private; use getters/setters for controlled access
- Prefer non-member non-friend functions to member functions to increase encapsulation
- When type conversions should apply to all parameters (including *this), use non-member functions
- Provide a non-throwing swap member function when std::swap would be inefficient for your type

### Implementation
- Postpone variable definitions as long as possible, ideally until you have initialization values
- Minimize casting; avoid dynamic_cast in performance-sensitive code
- When casting is necessary, hide it inside a function and prefer C++-style casts
- Never return handles (references, pointers, iterators) to private object internals
- Write exception-safe code with strong or nothrow guarantees; use copy-and-swap when appropriate
- Limit inlining to small, frequently called functions; don't inline just because functions are in headers
- Minimize compilation dependencies: depend on declarations, not definitions; use Handle/Interface classes

### Inheritance and OOP
- Ensure public inheritance always models "is-a" relationships
- Never hide inherited names; use using declarations or forwarding functions to make them visible
- Pure virtual functions specify interface only
- Simple virtual functions specify interface plus a default implementation
- Non-virtual functions specify interface plus a mandatory implementation
- Consider alternatives to virtual functions: NVI idiom, Strategy pattern, function pointers
- Never redefine inherited non-virtual functions
- Never redefine inherited default parameter values (they're statically bound)
- Use composition to model "has-a" or "is-implemented-in-terms-of" relationships
- Use private inheritance rarely and only when necessary; prefer composition
- Use multiple inheritance judiciously; be aware of ambiguity and virtual base class costs

### Templates and Generic Programming
- Understand that template interfaces are implicit and based on valid expressions
- Use typename to identify nested dependent type names in templates
- Access names in templatized base classes via this->, using declarations, or explicit base class qualification
- Factor parameter-independent code out of templates to reduce code bloat
- Use member function templates to accept all compatible types
- Define non-member functions inside class templates when type conversions are needed
- Use traits classes for type information available at compile time
- Use template metaprogramming to shift work from runtime to compile-time when beneficial

### Memory Management
- Understand set_new_handler behavior for handling memory allocation failures
- Consider custom new/delete operators for performance, debugging, or usage tracking
- operator new must contain an infinite loop, call new-handler on failure, and handle zero-byte requests
- operator delete must handle null pointers safely
- Write placement delete if you write placement new to prevent memory leaks
- Class-specific new/delete should handle requests for sizes different than expected

### General Practices
- Take compiler warnings seriously; compile warning-free at maximum warning level
- Don't depend on specific compiler warnings as they vary between compilers
- Know the standard library: STL, iostreams, locales, and standard C library
- Familiarize yourself with modern C++ features and best practices

## Modern C++ Coding Rules (C++11/14/17 and Beyond)

### Type Deduction (Items 1-4)
- Understand template type deduction rules: value categories, reference collapsing, and special cases
- Understand auto type deduction: mostly follows template rules but treats braced initializers as std::initializer_list
- Understand decltype: returns exact declared type; decltype(auto) deduces from initializer using decltype rules
- Know how to view deduced types: compiler diagnostics, runtime output (typeid, Boost.TypeIndex), or IDE tooltips

### Modern Type Declarations (Items 5-6)
- Prefer auto to explicit type declarations: reduces verbosity, ensures initialization, makes refactoring easier, and avoids type mismatches
- Use explicitly typed initializer idiom when auto deduces undesired types (e.g., proxy classes like vector<bool>::reference)

### Initialization and Declarations (Items 7-10)
- Distinguish between () and {} when creating objects: braces prevent narrowing, work everywhere, but beware of std::initializer_list overloads
- Prefer nullptr to 0 and NULL: type-safe, clearer intent, works with templates, enables function overloading
- Prefer alias declarations (using) to typedefs: work with templates, more readable, support template aliases
- Prefer scoped enums to unscoped enums: no implicit conversions, namespace pollution prevention, forward declarable, can specify underlying type

### Special Member Functions (Items 11-17)
- Prefer deleted functions (= delete) to private undefined ones: better error messages, works with any function (not just members), checked at compile-time
- Declare overriding functions override: catches interface mismatches, enables better refactoring, documents intent
- Prefer const_iterators to iterators: const-correctness, C++11 makes them practical with cbegin/cend
- Declare functions noexcept if they won't emit exceptions: enables optimizations (especially for move operations), required for some STL containers
- Use constexpr whenever possible: computed at compile-time, usable in constant expressions, broader scope than const
- Make const member functions thread-safe: use mutex for mutable data, consider std::atomic for simple cases
- Understand special member function generation: default constructor, destructor, copy ops, move ops; generation rules depend on what you declare

### Smart Pointers (Items 18-22)
- Use std::unique_ptr for exclusive-ownership resource management: zero overhead, move-only, perfect for factories, supports custom deleters
- Use std::shared_ptr for shared-ownership resource management: reference counted, thread-safe, larger overhead, use make_shared
- Use std::weak_ptr for std::shared_ptr-like pointers that can dangle: breaks reference cycles, cache implementations, observer patterns
- Prefer std::make_unique and std::make_shared to direct use of new: exception safety, efficiency (one allocation for shared_ptr), conciseness
- When using Pimpl Idiom, define special member functions in implementation file: required for unique_ptr with incomplete types

### Move Semantics and Perfect Forwarding (Items 23-30)
- Understand std::move and std::forward: move is unconditional cast to rvalue; forward is conditional cast preserving value category
- Distinguish universal references (T&&) from rvalue references: T&& is universal only with type deduction; rvalue reference otherwise
- Use std::move on rvalue references, std::forward on universal references: move enables moving; forward preserves lvalue/rvalue-ness
- Avoid overloading on universal references: they're too greedy and hijack overload resolution
- Familiarize yourself with alternatives to overloading on universal references: tag dispatch, enable_if, pass by value, trade-offs
- Understand reference collapsing: & + & = &, && + anything = that thing (except && + && = &&)
- Assume move operations are not present, not cheap, and not used: move isn't always generated, might not be faster, may not be called
- Familiarize yourself with perfect forwarding failure cases: braced initializers, null/0 pointers, declaration-only names, overloaded function names, bitfields

### Lambda Expressions (Items 31-34)
- Avoid default capture modes: [=] risks dangling pointers (especially to this); [&] risks dangling references and leads to lifetime issues
- Use init capture to move objects into closures: enables move-only types in captures, more efficient than copy capture
- Use decltype on auto&& parameters to std::forward them: enables generic lambdas to perfectly forward parameters
- Prefer lambdas to std::bind: more readable, easier to optimize, works better with overloading and templates

### Concurrency API (Items 35-40)
- Prefer task-based programming (std::async) to thread-based: automatic thread management, handles exceptions, returns futures
- Specify std::launch::async if asynchronicity is essential: default policy may run synchronously
- Make std::threads unjoinable on all paths: use RAII wrappers, join or detach before destruction to avoid termination
- Be aware of varying thread handle destructor behavior: std::thread destructs differently than std::future
- Consider void futures for one-shot event communication: condition variables for multiple waiters
- Use std::atomic for concurrency, volatile for special memory: atomic for thread-safe operations, volatile for hardware/signal handlers

### Tweaks and Best Practices (Items 41-42)
- Consider pass by value for copyable parameters that are cheap to move and always copied: one less copy, cleaner code
- Consider emplacement (emplace_back, emplace) instead of insertion: constructs in-place, avoids temporaries, more efficient

## Testing Practice
- Unit tests should be placed in the test/ directory matching source structure
- Use cmake test framework for unit tests
- Test names should be descriptive of what they're testing