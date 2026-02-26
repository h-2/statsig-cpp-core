#include "statsig.h"
#include "../include/libstatsig_ffi.h"
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

using json = nlohmann::json;

namespace deepl_statsig {

// ── Handle destructors / move semantics ─────────────────────────────────────

User::~User() {
    if (_ref != 0) statsig_user_release(_ref);
}
User::User(User&& o) noexcept : _ref(o._ref) { o._ref = 0; }
User& User::operator=(User&& o) noexcept {
    if (this != &o) {
        if (_ref != 0) statsig_user_release(_ref);
        _ref = o._ref;
        o._ref = 0;
    }
    return *this;
}

Options::~Options() {
    if (_ref != 0) statsig_options_release(_ref);
}
Options::Options(Options&& o) noexcept : _ref(o._ref) { o._ref = 0; }
Options& Options::operator=(Options&& o) noexcept {
    if (this != &o) {
        if (_ref != 0) statsig_options_release(_ref);
        _ref = o._ref;
        o._ref = 0;
    }
    return *this;
}

StatsigClient::~StatsigClient() {
    if (_ref != 0) statsig_release(_ref);
}
StatsigClient::StatsigClient(StatsigClient&& o) noexcept : _ref(o._ref) { o._ref = 0; }
StatsigClient& StatsigClient::operator=(StatsigClient&& o) noexcept {
    if (this != &o) {
        if (_ref != 0) statsig_release(_ref);
        _ref = o._ref;
        o._ref = 0;
    }
    return *this;
}

// ── Helpers ──────────────────────────────────────────────────────────────────

static EvaluationDetails parse_details(const json& j) {
    EvaluationDetails d;
    d.reason = j.value("reason", std::string{});
    if (j.contains("lcut") && j["lcut"].is_number())
        d.lcut = j["lcut"].get<uint64_t>();
    if (j.contains("received_at") && j["received_at"].is_number())
        d.received_at = j["received_at"].get<uint64_t>();
    return d;
}

static FeatureGate parse_feature_gate(const std::string& json_str) {
    const json j = json::parse(json_str);
    FeatureGate g;
    g.name    = j.value("name",    std::string{});
    g.value   = j.value("value",   false);
    g.rule_id = j.value("rule_id", std::string{});
    g.id_type = j.value("id_type", std::string{});
    if (j.contains("details") && j["details"].is_object())
        g.details = parse_details(j["details"]);
    return g;
}

static std::string gate_options_json(const CheckGateOptions& opts) {
    return json{{"disable_exposure_logging", opts.disable_exposure_logging}}.dump();
}

// ── Factory functions ────────────────────────────────────────────────────────

User make_user(const UserData& data) {
    json j = json::object();
    if (data.user_id)    j["userID"]    = *data.user_id;
    if (data.custom_ids) j["customIDs"] = *data.custom_ids;
    if (data.email)      j["email"]     = *data.email;
    if (data.ip)         j["ip"]        = *data.ip;
    if (data.country)    j["country"]   = *data.country;
    if (data.locale)     j["locale"]    = *data.locale;
    if (data.app_version) j["appVersion"] = *data.app_version;
    if (data.custom_json) {
        // custom_json is a pre-serialised JSON object string; embed it as-is.
        j["custom"] = json::parse(*data.custom_json);
    }
    if (data.private_attributes_json) {
        j["privateAttributes"] = json::parse(*data.private_attributes_json);
    }
    const std::string s = j.dump();
    return User{statsig_user_create_from_data(s.c_str())};
}

Options make_options(const OptionsData& data) {
    json j = json::object();
    if (data.specs_url)          j["specs_url"]          = *data.specs_url;
    if (data.log_event_url)      j["log_event_url"]      = *data.log_event_url;
    if (data.environment)        j["environment"]        = *data.environment;
    if (data.output_log_level)   j["output_log_level"]   = *data.output_log_level;
    if (data.disable_all_logging) j["disable_all_logging"] = *data.disable_all_logging;
    if (data.disable_network)    j["disable_network"]    = *data.disable_network;
    const std::string s = j.dump();
    return Options{statsig_options_create_from_data(s.c_str())};
}

StatsigClient make_client(const std::string& sdk_key, std::optional<Options> opts) {
    // Pass the options _ref if present. The FFI takes its own internal copy at
    // statsig_create time, so it is safe for the Options RAII handle to release
    // its ref normally after this call.
    const uint64_t opts_ref = opts ? opts->ref() : 0;
    return StatsigClient{statsig_create(sdk_key.c_str(), opts_ref)};
}

// ── Core API ─────────────────────────────────────────────────────────────────

void initialize_blocking(StatsigClient& client) {
    statsig_initialize_blocking(client.ref());
}

void shutdown_blocking(StatsigClient& client) {
    statsig_shutdown_blocking(client.ref());
}

bool check_gate(StatsigClient& client, const User& user,
                const std::string& gate_name, CheckGateOptions opts) {
    const std::string opts_str = gate_options_json(opts);
    return statsig_check_gate(client.ref(), user.ref(), gate_name.c_str(), opts_str.c_str());
}

FeatureGate get_feature_gate(StatsigClient& client, const User& user,
                             const std::string& gate_name, CheckGateOptions opts) {
    const std::string opts_str = gate_options_json(opts);
    char * fg = statsig_get_feature_gate(client.ref(), user.ref(),
                                           gate_name.c_str(), opts_str.c_str());
    if (!fg)
        return FeatureGate{};

    std::string fg_str(fg);
    free_string(fg);
    return parse_feature_gate(std::move(fg_str));
}

} // namespace deepl_statsig
