#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

namespace deepl_statsig {

// ── Opaque handles (move-only) ───────────────────────────────────────────────
//
// Each handle owns a reference counted by the Rust FFI layer. Copying is
// disabled; use std::move to transfer ownership.

class User {
    uint64_t _ref;
public:
    User(uint64_t r) : _ref(r) {}
    User(User&&) noexcept;
    User& operator=(User&&) noexcept;
    User(const User&) = delete;
    User& operator=(const User&) = delete;
    ~User();

    uint64_t ref() const { return _ref; }
};

class Options {
    uint64_t _ref;
public:
    Options(uint64_t r) : _ref(r) {}
    Options(Options&&) noexcept;
    Options& operator=(Options&&) noexcept;
    Options(const Options&) = delete;
    Options& operator=(const Options&) = delete;
    ~Options();

    uint64_t ref() const { return _ref; }
};

class StatsigClient {
    uint64_t _ref;
public:
    StatsigClient(uint64_t r) : _ref(r) {}
    StatsigClient(StatsigClient&&) noexcept;
    StatsigClient& operator=(StatsigClient&&) noexcept;
    StatsigClient(const StatsigClient&) = delete;
    StatsigClient& operator=(const StatsigClient&) = delete;
    ~StatsigClient();

    uint64_t ref() const { return _ref; }
};

// ── Value types (plain aggregates) ──────────────────────────────────────────

struct EvaluationDetails {
    std::string reason;
    std::optional<uint64_t> lcut;
    std::optional<uint64_t> received_at;
};

struct FeatureGate {
    std::string name;
    bool value = false;
    std::string rule_id;
    std::string id_type;
    EvaluationDetails details;
};

struct CheckGateOptions {
    bool disable_exposure_logging = false;
};

// ── Input data (plain aggregates) ────────────────────────────────────────────
//
// These are plain data structs used to construct the opaque handles above via
// the make_* factory functions. Brace-initialisation works for all fields.

struct UserData {
    std::optional<std::string> user_id{};
    std::optional<std::unordered_map<std::string, std::string>> custom_ids{};
    std::optional<std::string> email{};
    std::optional<std::string> ip{};
    std::optional<std::string> country{};
    std::optional<std::string> locale{};
    std::optional<std::string> app_version{};
    // Arbitrary JSON objects as pre-serialised strings, e.g. "{\"plan\":\"pro\"}"
    std::optional<std::string> custom_json{};
    std::optional<std::string> private_attributes_json{};
};

struct OptionsData {
    std::optional<std::string> specs_url{};
    std::optional<std::string> log_event_url{};
    std::optional<std::string> environment{};
    std::optional<std::string> output_log_level{};
    std::optional<bool> disable_all_logging{};
    std::optional<bool> disable_network{};
};

// ── Factory free functions ───────────────────────────────────────────────────

User make_user(const UserData& data);
Options make_options(const OptionsData& data);

// opts is consumed (moved) so the Options handle is not double-released.
StatsigClient make_client(const std::string& sdk_key,
                          std::optional<Options> opts = std::nullopt);

// ── Core API ─────────────────────────────────────────────────────────────────

void initialize_blocking(StatsigClient& client);
void shutdown_blocking(StatsigClient& client);

bool check_gate(const StatsigClient& client, const User& user,
                const std::string& gate_name,
                CheckGateOptions opts = {});

FeatureGate get_feature_gate(const StatsigClient& client, const User& user,
                             const std::string& gate_name,
                             CheckGateOptions opts = {});

} // namespace deepl_statsig
