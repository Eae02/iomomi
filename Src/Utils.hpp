#pragma once

#include <iostream>

//For strcasecmp
#if defined(__linux__)
#include <strings.h>
#elif defined(_WIN32)
#define strcasecmp _stricmp
#define strncasecmp _strnicmp 
#endif

#define BIT_FIELD(T) \
inline constexpr T operator|(T a, T b) noexcept \
{ return static_cast<T>(static_cast<int>(a) | static_cast<int>(b)); }\
inline constexpr T operator&(T a, T b) noexcept \
{ return static_cast<T>(static_cast<int>(a) & static_cast<int>(b)); }\
inline constexpr T& operator|=(T& a, T b) noexcept \
{ a = a | b; return a; }

#if defined(NDEBUG)
#define DEBUG_BREAK
#elif defined(_MSC_VER)
#define DEBUG_BREAK __debugbreak();
#elif defined(__linux__)
#include <signal.h>
#define DEBUG_BREAK raise(SIGTRAP);
#else
#define DEBUG_BREAK
#endif

#ifndef NDEBUG
#define UNREACHABLE std::abort();
#elif defined(__GNUC__)
#define UNREACHABLE __builtin_unreachable();
#else
#define UNREACHABLE
#endif

#ifdef NDEBUG
#define ASSERT(condition)
#define PANIC(msg) { std::ostringstream ps; ps << "A runtime error occured\nDescription: " << msg; ReleasePanic(ps.str()); }
#else
#define PANIC(msg) { std::cerr << "PANIC@" << __FILE__ << ":" << __LINE__ << "\n" << msg << std::endl; DEBUG_BREAK; std::abort(); }
#define ASSERT(condition) if (!(condition)) { std::cerr << "ASSERT@" << __FILE__ << ":" << __LINE__ << " " #condition << std::endl; DEBUG_BREAK; std::abort(); }
#endif

constexpr float PI = 3.141592653589793f;
constexpr float TWO_PI = 6.283185307179586f;
constexpr float HALF_PI = 1.5707963267948966f;

std::vector<char> ReadStreamContents(std::istream& stream);

/***
 * Deleter for use with unique_ptr which calls std::free
 */
struct FreeDel
{
	void operator()(void* mem) const
	{
		std::free(mem);
	}
};

/***
 * Checks the the given bitfield has a specific flag set.
 * @tparam T The type of the bitfield enum.
 * @param bits The bitfield to check.
 * @param flag The flag to check for.
 * @return Whether the bitfield has the flag set.
 */
template <typename T>
inline constexpr bool HasFlag(T bits, T flag)
{
	return (int)(bits & flag) != 0;
}

template <typename T, size_t I>
constexpr inline size_t ArrayLen(T (& array)[I])
{
	return I;
}

inline bool FEqual(float a, float b)
{
	return std::abs(a - b) < 1E-6f;
}

template <typename T, typename U>
constexpr inline auto RoundToNextMultiple(T value, U multiple)
{
	auto valModMul = value % multiple;
	return valModMul == 0 ? value : (value + multiple - valModMul);
}

inline bool StringEqualCaseInsensitive(std::string_view a, std::string_view b)
{
	return a.size() == b.size() && strncasecmp(a.data(), b.data(), a.size()) == 0;
}

inline glm::vec3 ParseRGBHexColor(int hex)
{
	glm::vec3 color;
	
	color.r = ((hex & 0xFF0000) >> (8 * 2)) / 255.0f;
	color.g = ((hex & 0x00FF00) >> (8 * 1)) / 255.0f;
	color.b = ((hex & 0x0000FF) >> (8 * 0)) / 255.0f;
	
	return color;
}

/**
 * Concatenates a list of string views into one string object.
 * @param list The list of parts to concatenate
 * @return The combined string
 */
std::string Concat(std::initializer_list<std::string_view> list);

/**
 * Removes whitespace from the start and end of the input string.
 * @param input The string to remove whitespace from.
 * @return Input without leading and trailing whitespace.
 */
std::string_view TrimString(std::string_view input);

uint64_t HashFNV1a64(std::string_view s);
uint32_t HashFNV1a32(std::string_view s);

inline bool StringEndsWith(std::string_view string, std::string_view suffix)
{
	return string.size() >= suffix.size() && string.compare(string.size() - suffix.size(), suffix.size(), suffix) == 0;
}

inline bool StringStartsWith(std::string_view string, std::string_view prefix)
{
	return string.size() >= prefix.size() && string.compare(0, prefix.size(), prefix) == 0;
}

/**
 * Invokes a callback for each parts of a string that is separated by a given delimiter. Empty parts are skipped.
 * @tparam CallbackTp The callback type, signature should be void(std::string_view)
 * @param string The string to loop through the parts of.
 * @param delimiter The delimiter to use.
 * @param callback The callback function.
 */
template <typename CallbackTp>
void IterateStringParts(std::string_view string, char delimiter, CallbackTp callback)
{
	for (size_t pos = 0; pos < string.size(); pos++)
	{
		const size_t end = string.find(delimiter, pos);
		if (end == pos)
			continue;
		
		const size_t partLen = end == std::string_view::npos ? std::string_view::npos : (end - pos);
		callback(string.substr(pos, partLen));
		
		if (end == std::string_view::npos)
			break;
		pos = end;
	}
}

void ReleasePanic(const std::string& message);

inline int8_t ToSNorm(float x)
{
	return (int8_t)glm::clamp((int)(x * 127), -127, 127);
}
