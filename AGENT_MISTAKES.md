# Agent Mistakes Log

Mistakes made by LLM agents during C++ development. Each entry describes what went wrong, why, and the rule to follow. Consult this file **before** writing any code.

**Core principle behind most entries**: The agent wrote code based on assumptions about how something worked instead of reading the actual source first.

---

## TOOL USAGE

### M001 — `replace_string_in_file` leaving duplicate function bodies
**What happened**: Matched only the opening line of a function in the search string. The tool replaced that line but left the old function body intact below it, producing a duplicate and compile errors. Happened multiple times on the same file in the same session even after being flagged.  
**Rule**: When replacing a function body, the search string must include enough of the body to uniquely identify AND fully consume the old content. If the function is large, replace the whole file. **After any replace on a `.cpp` file, immediately read back the region around the closing `} // namespace` brace to verify no duplicate tail remains.**

### M002 — Reformatting code unnecessarily
**What happened**: When rewriting a file, changed ~37 formatting decisions (brace placement, single-line if statements, spacing) that differed from the project's existing style, causing the user to have to manually revert them.  
**Rule**: Never reformat code that isn't part of the task. Match the existing file's style exactly. Only change lines that are necessary for the task. If in doubt, copy the surrounding style character-for-character.

---

## C++ LANGUAGE / LIBRARY

### M003 — Raw pointer into `std::vector` captured in a lambda
**What happened**: Captured `T* ptr = &vec.back()` in a lambda. A subsequent `vec.push_back()` reallocated the vector, invalidating `ptr` and causing undefined behaviour.  
**Rule**: Never capture a pointer or reference to a `std::vector` element in a lambda or callback that outlives the current scope. Capture a stable key (an id, a copy of the value, etc.) and look it up at call time.

### M004 — Calling `.message()` on a plain enum error type
**What happened**: Added diagnostic code calling `.message()` on an error value. The error type was a plain `enum class`, not a `std::error_code`, so it had no such method and failed to compile.  
**Rule**: Before calling any method on an error type, read its definition. Plain enums have no methods — use `static_cast<int>()` or a project-provided `to_string()`. Only `std::error_code` / `std::error_condition` have `.message()`.

### M005 — Using an RLP compact-encoding helper to decode a fixed-width ABI value
**What happened**: Used a helper designed for RLP's variable-length compact encoding (`from_big_compact`) to decode a 32-byte ABI word. The helper rejects leading zero bytes (an RLP canonicality rule), so small values with many leading zeros decoded as zero.  
**Rule**: Understand what encoding a helper was designed for before using it. ABI encoding uses fixed 32-byte words with leading zeros — a completely different contract from RLP's compact encoding. Use the right tool for each encoding format; do not cross them.

### M006 — Using a constructor or API without reading its actual signature
**What happened**: Attempted to construct `intx::uint256{lo128, hi128}` — no such constructor exists. Multiple further attempts with wrong argument counts before finally reading the header.  
**Rule**: **Read the actual header/source before writing a non-trivial construction or API call.** Do not guess constructors, method signatures, or template parameters. One read of the relevant file would have prevented three failed attempts. This is the most common root cause of wasted iterations.

### M007 — Assuming a return type has a standard interface without checking
**What happened**: Called `.begin()` / `.end()` directly on the return value of a crypto library hash function. The actual return type was an internal proxy object with no iterator interface.  
**Rule**: When using a third-party or unfamiliar library, read how the return type is actually used elsewhere in the codebase before writing new code against it. Find an existing usage example first, then follow that pattern exactly.

---

## DESIGN / LOGIC

### M008 — Re-broadcasting a filtered callback to all subscribers
**What happened**: An inner component (`EventWatcher`) already applied address and topic filters and called a single specific callback. That callback then looped over all subscriptions and re-dispatched to any whose signature hash matched — discarding the address filter the inner component had already applied. Two subscribers watching different contracts both fired for every log.  
**Rule**: When an inner component already does filtering and invokes a specific callback, that callback must act on behalf of exactly one subscription. Do not re-broadcast inside the callback — doing so throws away the filter results computed by the inner component.

---

## PROCESS

### P001 — Trying to observe runtime behaviour by adding debug prints, compiling, and running
**Rule**: Do not add debug strings to source, compile, and run to observe behaviour. This is a "try and see" loop that wastes build cycles. Instead: read the relevant source files, trace the logic statically, identify the root cause, then make a single targeted fix.

### P002 — Searching for things that are already known from the project structure
**Rule**: The project file tree is provided at the start of each session. Do not search for files or symbols that are already visible in the tree. Open and read the relevant file directly.

---

## DOCUMENTATION

### M009 — Stripping Doxygen comments during file rewrites
**What happened**: When rewriting a header file, replaced a fully documented block with a stripped-down version that omitted `@param`, `@return`, and descriptive Doxygen comments that were present in the original.  
**Root cause**: The rewrite focused on the code changes and did not preserve the existing documentation.  
**Rule**: When editing or rewriting any function declaration, always carry forward all existing Doxygen comments verbatim. Only add or modify comments that are directly related to the change being made. Never silently drop `@brief`, `@param`, `@return`, or `@note` tags.

