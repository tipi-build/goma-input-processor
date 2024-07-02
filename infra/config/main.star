#!/usr/bin/env lucicfg
# Copyright 2021 Google LLC. All Rights Reserved.

REPO_URL = "https://chromium.googlesource.com/infra/goma/client"

os_type = struct(
    LINUX = "Ubuntu-18.04",
    MAC = "Mac-11",
    WIN = "Windows-10",
)

# Use LUCI Scheduler BBv2 names and add Scheduler realms configs.
lucicfg.enable_experiment("crbug.com/1182002")

luci.project(
    name = "goma-client",
    buildbucket = "cr-buildbucket.appspot.com",
    logdog = "luci-logdog.appspot.com",
    milo = "luci-milo.appspot.com",
    scheduler = "luci-scheduler.appspot.com",
    swarming = "chromium-swarm.appspot.com",
    acls = [
        acl.entry(
            roles = [
                acl.BUILDBUCKET_READER,
                acl.PROJECT_CONFIGS_READER,
                acl.SCHEDULER_READER,
                acl.LOGDOG_READER,
            ],
            groups = "all",
        ),
        acl.entry(
            roles = [
                acl.CQ_COMMITTER,
            ],
            groups = "project-goma-client-committers",
        ),
        acl.entry(
            roles = [
                acl.SCHEDULER_OWNER,
            ],
            groups = "project-goma-client-admins",
        ),
        acl.entry(
            roles = [
                acl.CQ_DRY_RUNNER,
            ],
            groups = "project-goma-client-tryjob-access",
        ),
        acl.entry(
            roles = [
                acl.LOGDOG_WRITER,
            ],
            groups = "luci-logdog-chromium-writers",
        ),
    ],
)

# commit-queue
luci.cq(
    submit_max_burst = 4,
    submit_burst_delay = 480 * time.second,
    status_host = "chromium-cq-status.appspot.com",
)

luci.cq_group(
    name = "goma_client_cq",
    watch = cq.refset(
        repo = "https://chromium-review.googlesource.com/infra/goma/client",
        refs = ["refs/heads/.+"],
    ),
    tree_status_host = "infra-status.appspot.com",
    retry_config = cq.retry_config(
        single_quota = 1,
        global_quota = 2,
        failure_weight = 1,
        transient_failure_weight = 1,
        timeout_weight = 2,
    ),
    verifiers = [
        luci.cq_tryjob_verifier(
            builder = "try/linux_rel",
        ),
        luci.cq_tryjob_verifier(
            builder = "try/mac_rel",
        ),
        luci.cq_tryjob_verifier(
            builder = "try/win_rel",
        ),
    ],
)

# cr-buildbucket
luci.bucket(
    name = "try",
    acls = [
        # Allow launching tryjobs directly (in addition to doing it through CQ).
        acl.entry(
            roles = acl.BUILDBUCKET_TRIGGERER,
            groups = "project-goma-client-tryjob-access",
        ),
    ],
    bindings = [
        luci.binding(
            roles = "role/swarming.taskTriggerer",
            groups = "flex-internal-try-led-users",
        ),
    ],
)

luci.bucket(
    name = "ci",
    acls = [
        acl.entry(
            roles = acl.BUILDBUCKET_TRIGGERER,
            users = "luci-scheduler@appspot.gserviceaccount.com",
        ),
    ],
    bindings = [
        luci.binding(
            roles = "role/swarming.taskTriggerer",
            groups = "flex-internal-try-led-users",
        ),
    ],
)

luci.gitiles_poller(
    name = "gitiles-trigger",
    bucket = "ci",
    repo = REPO_URL,
)

def builder(name, os, bucket):
    triggered_by = None
    if bucket == "ci":
        triggered_by = ["gitiles-trigger"]

    caches = None
    if os == os_type.MAC:
        caches = [swarming.cache("osx_sdk")]

    return luci.builder(
        name = name,
        bucket = bucket,
        executable = luci.recipe(
            name = "goma_client",
            cipd_package = "infra/recipe_bundles/chromium.googlesource.com/" +
                           "chromium/tools/build",
            use_bbagent = True,
        ),
        service_account = "goma-client-ext-" + bucket + "-builders@" +
                          "chops-service-accounts.iam.gserviceaccount.com",
        caches = caches,
        dimensions = {
            "cpu": "x86-64",
            "os": os,
            "pool": "luci.flex." + bucket,
        },
        triggered_by = triggered_by,
        experiments = {
            "luci.recipes.use_python3": 100,
        },
    )

builder("linux_rel", os_type.LINUX, "ci")
builder("linux_rel", os_type.LINUX, "try")
builder("mac_rel", os_type.MAC, "ci")
builder("mac_rel", os_type.MAC, "try")
builder("win_rel", os_type.WIN, "ci")
builder("win_rel", os_type.WIN, "try")

# luci-logdog
luci.logdog(
    gs_bucket = "chromium-luci-logdog",
)

# luci-milo
luci.milo(
    logo = "https://storage.googleapis.com/chrome-infra-public/logo/goma-logo.png",
)

luci.list_view(
    name = "try builders of Goma client",
    entries = [
        "try/linux_rel",
        "try/mac_rel",
        "try/win_rel",
    ],
)

luci.console_view(
    name = "Goma client repo console",
    repo = REPO_URL,
    entries = [
        luci.console_view_entry(
            builder = "ci/linux_rel",
            short_name = "linux",
        ),
        luci.console_view_entry(
            builder = "ci/mac_rel",
            short_name = "mac",
        ),
        luci.console_view_entry(
            builder = "ci/win_rel",
            short_name = "win",
        ),
    ],
)
