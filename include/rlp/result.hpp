#ifndef RLP_RESULT_HPP
#define RLP_RESULT_HPP

#include "errors.hpp"
#include <boost/outcome/result.hpp>
#include <boost/outcome/try.hpp>

namespace rlp {

namespace outcome = BOOST_OUTCOME_V2_NAMESPACE;

// Result types for encoding operations
template <class T>
using EncodingResult = outcome::result<T, EncodingError, outcome::policy::all_narrow>;

using EncodingOperationResult = outcome::result<void, EncodingError, outcome::policy::all_narrow>;

// Result types for decoding operations
template <class T>
using Result = outcome::result<T, DecodingError, outcome::policy::all_narrow>;

using DecodingResult = outcome::result<void, DecodingError, outcome::policy::all_narrow>;

// Result types for streaming operations
template <class T>
using StreamingResult = outcome::result<T, StreamingError, outcome::policy::all_narrow>;

using StreamingOperationResult = outcome::result<void, StreamingError, outcome::policy::all_narrow>;

} // namespace rlp

#endif // RLP_RESULT_HPP
