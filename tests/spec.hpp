// A tiny executable-specification harness shared by every test binary: scenarios in
// Given/When/Then form, soft checks that report and keep going, and a non-zero exit code when
// anything failed. No dependencies, so the suites build everywhere the plug-in does.
#pragma once

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

namespace spec {

struct Entry { std::string name; std::function<void()> body; };
inline std::vector<Entry>& registry() { static std::vector<Entry> r; return r; }
inline int& failures() { static int f = 0; return f; }
inline int& checks() { static int c = 0; return c; }
inline std::string& current() { static std::string s; return s; }

struct Registrar {
  Registrar(const char* name, std::function<void()> body) { registry().push_back({name, std::move(body)}); }
};

inline void step(const char* verb, const std::string& text) { std::printf("  %-6s %s\n", verb, text.c_str()); }
inline void given(const std::string& t) { step("Given", t); }
inline void when(const std::string& t) { step("When", t); }
inline void then(const std::string& t) { step("Then", t); }

inline bool check(bool condition, const std::string& what, const char* file, int line) {
  ++checks();
  if (condition) return true;
  ++failures();
  std::printf("    FAILED: %s  (%s:%d)\n", what.c_str(), file, line);
  return false;
}

inline std::string fmt(const char* format, double a, double b = 0.0, double c = 0.0) {
  char out[256];
  std::snprintf(out, sizeof(out), format, a, b, c);
  return out;
}

// Runs every scenario, or those whose names contain the filter, and returns the exit code.
inline int run(int argc, char** argv, int first_filter_arg = 1) {
  const char* filter = argc > first_filter_arg ? argv[first_filter_arg] : nullptr;
  // Unbuffered, so a crash still shows how far the run got.
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  int ran = 0;
  for (const Entry& entry : registry()) {
    if (filter && entry.name.find(filter) == std::string::npos) continue;
    current() = entry.name;
    std::printf("Scenario: %s\n", entry.name.c_str());
    const int before = failures();
    entry.body();
    std::printf("  %s\n", failures() == before ? "ok" : "FAILED");
    ++ran;
  }
  std::printf("%d scenario(s), %d check(s), %d failure(s)\n", ran, checks(), failures());
  return failures() == 0 && ran > 0 ? 0 : 1;
}

}  // namespace spec

#define SCENARIO(name) \
  static void scenario_##name(); \
  static ::spec::Registrar registrar_##name(#name, &scenario_##name); \
  static void scenario_##name()
#define GIVEN(text) ::spec::given(text)
#define WHEN(text) ::spec::when(text)
#define THEN(text) ::spec::then(text)
#define CHECK(condition, what) ::spec::check((condition), (what), __FILE__, __LINE__)
#define CHECK_NEAR(value, expected, tolerance, what) \
  ::spec::check(std::abs((value) - (expected)) <= (tolerance), \
                std::string(what) + ::spec::fmt(": got %.4f, expected %.4f ± %.4f", (value), (expected), (tolerance)), \
                __FILE__, __LINE__)
