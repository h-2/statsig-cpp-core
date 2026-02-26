// statsig_test.cpp — unit and end-to-end tests for deepl_statsig
//
// Environment variables required for E2E tests:
//
//   STATSIG_SDK_KEY   — server secret key for your Statsig project
//                       (format: "secret-xxxx...")
//
// E2E tests are skipped automatically when these variables are not set.
//
// Distinguishing "gate not defined" from "gate defined but always false"
// -----------------------------------------------------------------------
// Both cases return value=false from check_gate / get_feature_gate.
// The difference is exposed by FeatureGate::details.reason:
//
//   • Gate defined, always false  → reason is a data-source label such as
//     "NetworkDataAdapter" or "Bootstrap".  The gate exists in the ruleset
//     and was evaluated; the rule decided it should pass nobody.
//
//   • Gate not defined at all     → reason is "Unrecognized".  The SDK could
//     not find the gate name in the downloaded ruleset and returned the safe
//     default (false) without any rule evaluation.
//
// Use get_feature_gate() instead of check_gate() whenever you need to tell
// these two apart.

#include "../src/statsig.h"
#include <gtest/gtest.h>
#include <cstdlib>
#include <string>

using namespace deepl_statsig;

// ── Helpers ──────────────────────────────────────────────────────────────────

static const char* env(const char* name) {
    return std::getenv(name);
}

// ── Unit tests (no network) ───────────────────────────────────────────────────

TEST(StatsigUnit, MakeUser_MinimalUser_DoesNotCrash) {
    UserData data;
    data.user_id = "test-user-1";
    User u = make_user(data);
    EXPECT_NE(u.ref(), 0u);
}

TEST(StatsigUnit, MakeUser_WithCustomIDs_DoesNotCrash) {
    UserData data;
    data.custom_ids = {{"companyID", "acme"}, {"teamID", "eng"}};
    User u = make_user(data);
    EXPECT_NE(u.ref(), 0u);
}

TEST(StatsigUnit, MakeUser_AllFields_DoesNotCrash) {
    UserData data;
    data.user_id    = "test-user-2";
    data.email      = "test@example.com";
    data.ip         = "1.2.3.4";
    data.country    = "DE";
    data.locale     = "de-DE";
    data.app_version = "1.0.0";
    data.custom_json = R"({"plan":"pro","seats":10})";
    data.private_attributes_json = R"({"internal_id":"abc123"})";
    User u = make_user(data);
    EXPECT_NE(u.ref(), 0u);
}

TEST(StatsigUnit, MakeUser_Move_TransfersOwnership) {
    UserData data;
    data.user_id = "move-test";
    User a = make_user(data);
    const uint64_t original_ref = a.ref();
    User b = std::move(a);
    EXPECT_EQ(a.ref(), 0u);
    EXPECT_EQ(b.ref(), original_ref);
}

TEST(StatsigUnit, MakeOptions_Defaults_DoesNotCrash) {
    OptionsData data;
    Options o = make_options(data);
    EXPECT_NE(o.ref(), 0u);
}

TEST(StatsigUnit, MakeOptions_WithValues_DoesNotCrash) {
    OptionsData data;
    data.output_log_level   = "none";
    data.disable_all_logging = true;
    data.disable_network    = true;
    Options o = make_options(data);
    EXPECT_NE(o.ref(), 0u);
}

TEST(StatsigUnit, MakeClient_NoOptions_DoesNotCrash) {
    StatsigClient c = make_client("secret-placeholder");
    EXPECT_NE(c.ref(), 0u);
}

TEST(StatsigUnit, MakeClient_WithOptions_DoesNotCrash) {
    OptionsData od;
    od.disable_all_logging = true;
    od.disable_network     = true;
    StatsigClient c = make_client("secret-placeholder", make_options(od));
    EXPECT_NE(c.ref(), 0u);
}

TEST(StatsigUnit, StatsigClient_Move_TransfersOwnership) {
    StatsigClient a = make_client("secret-placeholder");
    const uint64_t original_ref = a.ref();
    StatsigClient b = std::move(a);
    EXPECT_EQ(a.ref(), 0u);
    EXPECT_EQ(b.ref(), original_ref);
}

// ── E2E tests (require STATSIG_SDK_KEY and STATSIG_test_gate_always_true) ────────────────

class StatsigE2E : public ::testing::Test {
protected:
    static std::string sdk_key;
    static std::string const test_gate_always_true;
    static std::string const test_gate_always_false;

    static void SetUpTestSuite() {
        const char* k = env("STATSIG_SDK_KEY");
        if (k) sdk_key   = k;
    }

    void SetUp() override {
        if (sdk_key.empty()) {
            GTEST_SKIP() << "Set STATSIG_SDK_KEY to run E2E tests";
        }
    }

    // Convenience: build a client that is already initialised.
    StatsigClient make_live_client() {
        OptionsData od;
        od.output_log_level = "none";
        StatsigClient c = make_client(sdk_key, make_options(od));
        initialize_blocking(c);
        return c;
    }

    User make_test_user(const std::string& id = "cpp-e2e-test-user") {
        return make_user(UserData{.user_id = id});
    }
};

std::string StatsigE2E::sdk_key;
std::string const StatsigE2E::test_gate_always_true = "debug_always_true";
std::string const StatsigE2E::test_gate_always_false = "debug_always_false";

TEST_F(StatsigE2E, InitializeAndShutdown_Succeeds) {
    StatsigClient c = make_client(sdk_key);
    initialize_blocking(c);
    shutdown_blocking(c);
}

TEST_F(StatsigE2E, CheckGate_KnownPassGate_ReturnsTrue) {
    StatsigClient c = make_live_client();
    User u = make_test_user();
    const bool result = check_gate(c, u, test_gate_always_true);
    EXPECT_TRUE(result);
    shutdown_blocking(c);
}

TEST_F(StatsigE2E, CheckGate_AlwaysFalseGate_ReturnsFalse) {
    // Gate exists in the project but its rules pass nobody.
    // check_gate() returns false — indistinguishable from "not defined" at
    // this level.  Use GetFeatureGate_* tests below to tell the two apart.
    StatsigClient c = make_live_client();
    User u = make_test_user();
    const bool result = check_gate(c, u, test_gate_always_false);
    EXPECT_FALSE(result);
    shutdown_blocking(c);
}

TEST_F(StatsigE2E, CheckGate_UndefinedGate_ReturnsFalse) {
    // Gate does not exist in the project at all.
    // check_gate() also returns false — same surface behaviour as an
    // always-false gate.  Use GetFeatureGate_* tests below to tell them apart.
    StatsigClient c = make_live_client();
    User u = make_test_user();
    const bool result = check_gate(c, u, "this_gate_does_not_exist_xyz");
    EXPECT_FALSE(result);
    shutdown_blocking(c);
}

// ── Distinguishing "always false" from "not defined" via get_feature_gate ────
//
// The only reliable way to tell these two cases apart is to inspect
// FeatureGate::details.reason:
//   • "Unrecognized"          → gate name is not in the downloaded ruleset
//   • anything else           → gate exists; the rule evaluated to false

TEST_F(StatsigE2E, GetFeatureGate_AlwaysFalseGate_ReasonIsNotUnrecognized) {
    // Gate is defined and deliberately passes nobody.  The SDK evaluates it
    // against the ruleset, so reason will be a data-source label (e.g.
    // "NetworkDataAdapter"), never "Unrecognized".
    StatsigClient c = make_live_client();
    User u = make_test_user();
    FeatureGate gate = get_feature_gate(c, u, test_gate_always_false);
    EXPECT_EQ(gate.name, test_gate_always_false);
    EXPECT_FALSE(gate.value);
    EXPECT_NE(gate.details.reason, "Network:Unrecognized")
        << "A defined gate should never have reason=\"Network:Unrecognized\"; "
           "got: " << gate.details.reason;
}

TEST_F(StatsigE2E, GetFeatureGate_UndefinedGate_ReasonIsUnrecognized) {
    // Gate does not exist in the project.  The SDK cannot evaluate it and
    // returns the safe default (false) with reason="Unrecognized".
    StatsigClient c = make_live_client();
    User u = make_test_user();
    FeatureGate gate = get_feature_gate(c, u, "this_gate_does_not_exist_xyz");
    EXPECT_FALSE(gate.value);
    EXPECT_EQ(gate.details.reason, "Network:Unrecognized")
        << "An undefined gate should have reason=\"Network:Unrecognized\"; "
           "got: " << gate.details.reason;
}

TEST_F(StatsigE2E, GetFeatureGate_PopulatesAllFields) {
    StatsigClient c = make_live_client();
    User u = make_test_user();
    FeatureGate gate = get_feature_gate(c, u, test_gate_always_true);
    EXPECT_EQ(gate.name, test_gate_always_true);
    EXPECT_TRUE(gate.value);
    EXPECT_FALSE(gate.details.reason.empty());
    shutdown_blocking(c);
}

TEST_F(StatsigE2E, GetFeatureGate_DisableExposure_SameValue) {
    StatsigClient c = make_live_client();
    User u = make_test_user("cpp-e2e-no-exposure-user");
    CheckGateOptions opts;
    opts.disable_exposure_logging = true;
    FeatureGate gate = get_feature_gate(c, u, test_gate_always_true, opts);
    EXPECT_EQ(gate.name, test_gate_always_true);
    EXPECT_TRUE(gate.value);
    shutdown_blocking(c);
}

TEST_F(StatsigE2E, CheckGate_MatchesGetFeatureGate_Value) {
    StatsigClient c = make_live_client();
    User u = make_test_user();
    const bool via_check = check_gate(c, u, test_gate_always_true);
    FeatureGate gate = get_feature_gate(c, u, test_gate_always_true);
    EXPECT_EQ(via_check, gate.value);
    shutdown_blocking(c);
}
