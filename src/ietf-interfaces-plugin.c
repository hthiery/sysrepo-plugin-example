#include <unistd.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdbool.h>

#include <sysrepo.h>
#include <sysrepo/values.h>
#include <sysrepo/xpath.h>

#define PLUGIN_NAME "IETF-INTF"

#define IETF_LOG_WRN(...)  SRPLG_LOG_WRN(PLUGIN_NAME, __VA_ARGS__)
#define IETF_LOG_INF(...)  SRPLG_LOG_INF(PLUGIN_NAME, __VA_ARGS__)
#define IETF_LOG_DBG(...)  SRPLG_LOG_DBG(PLUGIN_NAME, __VA_ARGS__)
#define IETF_LOG_ERR(...)  SRPLG_LOG_ERR(PLUGIN_NAME, __VA_ARGS__)

#define BASE_SYSTEM_YANG_MODEL "ietf-system"

#define BASE_INTF_YANG_MODEL "ietf-interfaces"

#define BASE_BRIDGE_YANG_MODEL "ieee802-dot1q-bridge"

#define XPATH_MAX_LEN  256


struct plugin_ctx {
	sr_subscription_ctx_t *sr_subscription;
	void *interfaces;
	void *intf_backend;
};

struct plugin_ctx global_ctx;

/* Text representation of Sysrepo event code. */
static const char * ev_to_str(sr_event_t event) {
	switch (event) {
	case SR_EV_UPDATE:
		return "UPDATE";
	case SR_EV_CHANGE:
		return "CHANGE";
	case SR_EV_DONE:
		return "DONE";
	case SR_EV_ABORT:
		return "ABORT";
	case SR_EV_ENABLED:
		return "ENABLED";
	case SR_EV_RPC:
		return "RPC";
	default:
		return "unknown";
	}
}

/* Text representation of Sysrepo event code. */
static const char * ds_to_str(sr_datastore_t ds) {
	switch (ds) {
	case SR_DS_STARTUP:
		return "startup";
	case SR_DS_RUNNING:
		return "running";
	case SR_DS_CANDIDATE:
		return "candidate";
	case SR_DS_OPERATIONAL:
		return "operational";
	default:
		return "unknown";
	}
}

static int set_interface_config_value(void *intfs,
		const char *xpath, const char *value)
{
	int rc = SR_ERR_OK;
	char *intf_node = NULL;
	char *intf_node_name = NULL;
	sr_xpath_ctx_t state = {0};
	char *xpath_cpy = strdup(xpath);

	struct interface *intf;

	intf_node = sr_xpath_node_name((char *) xpath);
	if (intf_node == NULL) {
		IETF_LOG_ERR("sr_xpath_node_name error");
		rc = SR_ERR_CALLBACK_FAILED;
		goto out;
	}

	intf_node_name = sr_xpath_key_value((char *) xpath, "interface", "name", &state);
	if (intf_node_name == NULL) {
		IETF_LOG_ERR("sr_xpath_key_value error");
		rc = SR_ERR_CALLBACK_FAILED;
		goto out;
	}
printf("### intf_node=%s\n", intf_node);
printf("### intf_node_name=%s\n", intf_node_name);

error_out:

	goto out;

out:

	return rc ? SR_ERR_CALLBACK_FAILED : SR_ERR_OK;
}

static int delete_interface_config_value(const char *xpath, const char *value)
{
	(void)xpath;
	(void)value;
	return SR_ERR_OK;
}

static int interfaces_module_change_cb(sr_session_ctx_t *session,
		uint32_t subscription_id, const char *module_name, const char *xpath,
		sr_event_t event, uint32_t request_id, void *private_data)
{
	struct plugin_ctx *ctx = (struct plugin_ctx*)private_data;
	int error = 0;
	sr_change_iter_t *change_iter = NULL;
	sr_change_oper_t operation = SR_OP_CREATED;
	const struct lyd_node *node = NULL;
	const char *prev_value = NULL;
	const char *prev_list = NULL;
	int prev_default = 0;

	(void)subscription_id;

	IETF_LOG_INF("%s: module_name: %s, xpath: %s, event: %s, request_id: %u",
			__func__, module_name, xpath, ev_to_str(event), request_id);

	if (event == SR_EV_ABORT) {
		IETF_LOG_ERR("aborting changes for: %s", xpath);
		error = -1;
		goto error_out;
	}

	if (event == SR_EV_DONE) {
		error = sr_copy_config(session, BASE_INTF_YANG_MODEL, SR_DS_RUNNING, 0);
		if (error) {
			IETF_LOG_ERR("sr_copy_config error (%d): %s", error, sr_strerror(error));
			goto error_out;
		}
	}
	if (event == SR_EV_CHANGE) {
		error = sr_get_changes_iter(session, xpath, &change_iter);
		if (error) {
			IETF_LOG_ERR("sr_get_changes_iter error (%d): %s", error, sr_strerror(error));
			goto error_out;
		}
		while (sr_get_change_tree_next(session, change_iter, &operation,
					&node, &prev_value, &prev_list, &prev_default) == SR_ERR_OK) {
			char *node_xpath = NULL, *node_value = NULL;

			node_xpath = lyd_path(node, LYD_PATH_STD, NULL, 0);

			if (node->schema->nodetype == LYS_LEAF
					|| node->schema->nodetype == LYS_LEAFLIST) {
				node_value = strdup(lyd_get_value(node));
			}

			IETF_LOG_DBG("%s: xpath: %s; prev_val: %s; node_val: %s; operation: %d",
					__func__, node_xpath, prev_value, node_value, operation);

			if (node->schema->nodetype == LYS_LEAF
					|| node->schema->nodetype == LYS_LEAFLIST) {
				if (operation == SR_OP_CREATED || operation == SR_OP_MODIFIED) {
					error = set_interface_config_value(ctx->interfaces, node_xpath, node_value);
					if (error) {
						IETF_LOG_ERR("set_interface_config_value error (%d)", error);
						goto error_out;
					}
				} else if (operation == SR_OP_DELETED) {
					error = delete_interface_config_value(node_xpath, node_value);
					if (error) {
						IETF_LOG_ERR("delete_interface_config_value error (%d)", error);
						goto error_out;
					}
				}
			}
			free(node_xpath);
			free(node_value);
		}

		if (error) {
			error = SR_ERR_CALLBACK_FAILED;
			IETF_LOG_ERR("update_link_info error");
			goto error_out;
		}
	}
	goto out;

error_out:
	// nothing for now
out:
	if (change_iter != NULL) {
		sr_free_change_iter(change_iter);
	}

	return error ? SR_ERR_CALLBACK_FAILED : SR_ERR_OK;
}

static int interfaces_state_data_cb(sr_session_ctx_t *session,
		uint32_t subscription_id, const char *module_name, const char *path,
		const char *request_xpath, uint32_t request_id,
		struct lyd_node **parent, void *private_data)
{
	struct plugin_ctx *ctx = (struct plugin_ctx*) private_data;
	int rc = SR_ERR_OK;
	const struct ly_ctx *ly_ctx = NULL;
	struct interface *intf;
	char xpath_buffer[XPATH_MAX_LEN] = {0};
	char intf_xpath_buffer[XPATH_MAX_LEN] = {0};
	char gate_xpath_buffer[XPATH_MAX_LEN] = {0};

	(void)subscription_id;
	(void)module_name;
	(void)request_id;

	IETF_LOG_INF("%s: %d: path=%s, req_xpath=%s, DS=%s", __func__, __LINE__,
				 path, request_xpath, ds_to_str(sr_session_get_ds(session)));

	goto out;

error_out:
	IETF_LOG_ERR("%s: %d\n", __func__, __LINE__);
	rc = SR_ERR_CALLBACK_FAILED;

out:
	return rc ? SR_ERR_CALLBACK_FAILED : SR_ERR_OK;
}

static int set_bridge_config_value(void *intfs,
		const char *xpath, const char *value)
{
	int rc = SR_ERR_OK;
	char *bridge_node = NULL;
	char *bridge_node_name = NULL;
	sr_xpath_ctx_t state = {0};
	char *xpath_cpy = strdup(xpath);

	struct interface *intf_bridge;

	bridge_node = sr_xpath_node_name((char *) xpath);
	if (bridge_node == NULL) {
		IETF_LOG_ERR("sr_xpath_node_name error");
		rc = SR_ERR_CALLBACK_FAILED;
		goto out;
	}

	bridge_node_name = sr_xpath_key_value((char *) xpath, "bridge", "name", &state);
	if (bridge_node_name == NULL) {
		IETF_LOG_ERR("sr_xpath_key_value error for bridge/name");
		rc = SR_ERR_CALLBACK_FAILED;
		goto out;
	}

error_out:
	goto out;

out:
	return rc;
}

int delete_bridge_config_value(const char *xpath, const char *value)
{
	(void)xpath;
	(void)value;
//	IETF_LOG_ERR("%s: %d", __func__, __LINE__);
	return SR_ERR_OK;
}

static int bridge_module_change_cb(sr_session_ctx_t *session,
		uint32_t subscription_id, const char *module_name, const char *xpath,
		sr_event_t event, uint32_t request_id, void *private_data)
{
	struct plugin_ctx *ctx = (struct plugin_ctx*)private_data;
	int error = 0;
	sr_change_iter_t *change_iter = NULL;
	sr_change_oper_t operation = SR_OP_CREATED;
	const struct lyd_node *node = NULL;
	const char *prev_value = NULL;
	const char *prev_list = NULL;
	int prev_default = 0;

	(void)subscription_id;

	IETF_LOG_INF("%s: module_name: %s, xpath: %s, event: %s, request_id: %u",
			__func__, module_name, xpath, ev_to_str(event), request_id);

	if (event == SR_EV_ABORT) {
		IETF_LOG_ERR("aborting changes for: %s", xpath);
		error = -1;
		goto error_out;
	}

	if (event == SR_EV_DONE) {
		error = sr_copy_config(session, BASE_BRIDGE_YANG_MODEL, SR_DS_RUNNING, 0);
		if (error) {
			IETF_LOG_ERR("sr_copy_config error (%d): %s", error, sr_strerror(error));
			goto error_out;
		}
	}

	if (event == SR_EV_CHANGE) {
		error = sr_get_changes_iter(session, "//.", &change_iter);
		if (error) {
			IETF_LOG_ERR("sr_get_changes_iter error (%d): %s", error, sr_strerror(error));
			goto error_out;
		}

		while (sr_get_change_tree_next(session, change_iter, &operation,
					&node, &prev_value, &prev_list, &prev_default) == SR_ERR_OK) {
			char *node_xpath = NULL, *node_value = NULL;

			node_xpath = lyd_path(node, LYD_PATH_STD, NULL, 0);

			if (node->schema->nodetype == LYS_LEAF
					|| node->schema->nodetype == LYS_LEAFLIST) {
				node_value = strdup(lyd_get_value(node));
			}

			IETF_LOG_DBG("%s: xpath: %s; prev_val: %s; node_val: %s; operation: %d",
					__func__, node_xpath, prev_value, node_value, operation);

			if (node->schema->nodetype == LYS_LEAF
					|| node->schema->nodetype == LYS_LEAFLIST) {
				if (operation == SR_OP_CREATED || operation == SR_OP_MODIFIED) {
					error = set_bridge_config_value(ctx->interfaces, node_xpath, node_value);
					if (error) {
						IETF_LOG_ERR("set_bridge_config_value error (%d)", error);
						goto error_out;
					}
				} else if (operation == SR_OP_DELETED) {
					error = delete_bridge_config_value(node_xpath, node_value);
					if (error) {
						IETF_LOG_ERR("delete_bridge_config_value error (%d)", error);
						goto error_out;
					}
				}
			}
			free(node_xpath);
			free(node_value);
		}

		if (error) {
			error = SR_ERR_CALLBACK_FAILED;
			IETF_LOG_ERR("update_link_info error");
			goto error_out;
		}
	}

	goto out;

error_out:
	// nothing for now
out:
	if (change_iter != NULL) {
		sr_free_change_iter(change_iter);
	}

	return error ? SR_ERR_CALLBACK_FAILED : SR_ERR_OK;
}

static int bridge_state_data_cb(sr_session_ctx_t *session,
		uint32_t subscription_id, const char *module_name, const char *path,
		const char *request_xpath, uint32_t request_id,
		struct lyd_node **parent, void *private_data)
{
	struct plugin_ctx *ctx = (struct plugin_ctx*) private_data;
	int rc = SR_ERR_OK;
	const struct ly_ctx *ly_ctx = NULL;
	char *if_name;
	struct interface *intf;
	char xpath_buffer[XPATH_MAX_LEN] = {0};
	char bridge_xpath_buffer[XPATH_MAX_LEN] = {0};
	char component_xpath_buffer[XPATH_MAX_LEN] = {0};

	(void)subscription_id;
	(void)module_name;
	(void)request_id;

	IETF_LOG_INF("%s: %d: path=%s, req_xpath=%s, DS=%s", __func__, __LINE__,
				 path, request_xpath, ds_to_str(sr_session_get_ds(session)));

	goto out;

error_out:
	IETF_LOG_ERR("%s: %d\n", __func__, __LINE__);
	rc = SR_ERR_CALLBACK_FAILED;

out:
	return rc ? SR_ERR_CALLBACK_FAILED : SR_ERR_OK;
}

/* Callback called by plugin deamon. */
int sr_plugin_init_cb(sr_session_ctx_t *session, void **private_ctx)
{
	int rc;
	struct plugin_ctx *ctx = NULL;

	IETF_LOG_DBG("%s: Initializing interfaces plugin.", __func__);

	ctx = &global_ctx;
	*private_ctx = ctx;

	/* Interface callbacks */
	rc = sr_module_change_subscribe(session,
			BASE_INTF_YANG_MODEL, "/ietf-interfaces:interfaces//*",
			interfaces_module_change_cb, *private_ctx, 100,
			SR_SUBSCR_ENABLED, &ctx->sr_subscription);
	if (rc) {
		IETF_LOG_ERR("sr_module_change_subscribe error (%d): %s",
				rc, sr_strerror(rc));
		goto error;
	}

	rc = sr_oper_get_subscribe(session,
			BASE_INTF_YANG_MODEL, "/ietf-interfaces:*/*",
			interfaces_state_data_cb, *private_ctx, SR_SUBSCR_UPDATE,
			&ctx->sr_subscription);
	if (rc) {
		IETF_LOG_ERR("sr_oper_get_items_subscribe error (%d): %s",
				rc , sr_strerror(rc));
		goto error;
	}

	/* Bridge callbacks */
	rc = sr_module_change_subscribe(session,
			BASE_BRIDGE_YANG_MODEL, "/ieee802-dot1q-bridge:bridges//*",
			bridge_module_change_cb, *private_ctx, 100,
			SR_SUBSCR_ENABLED, &ctx->sr_subscription);
	if (rc) {
		IETF_LOG_ERR("sr_module_change_subscribe error (%d): %s",
				rc, sr_strerror(rc));
		goto error;
	}

	rc = sr_oper_get_subscribe(session,
			BASE_BRIDGE_YANG_MODEL, "/ieee802-dot1q-bridge:bridges/*",
			bridge_state_data_cb, *private_ctx, SR_SUBSCR_UPDATE,
			&ctx->sr_subscription);
	if (rc) {
		IETF_LOG_ERR("sr_oper_get_items_subscribe error (%d): %s",
				rc , sr_strerror(rc));
		goto error;
	}

	IETF_LOG_INF("%s: interfaces plugin initialized successfully.", __func__);

	return 0;

error:
	if (ctx->sr_subscription != NULL) {
		sr_unsubscribe(ctx->sr_subscription);
	}

	IETF_LOG_ERR("%s: Error by initialization of the interfaces plugin.", __func__);

	/* cleanup will be done during application shutdown */
	return rc;
}

/* Callback called by plugin deamon. */
void sr_plugin_cleanup_cb(sr_session_ctx_t *session, void *private_data)
{
	struct plugin_ctx *ctx = (struct plugin_ctx*) private_data;
	(void)session;
	IETF_LOG_DBG("%s: Cleanup of interfaces plugin requested.", __func__);
	sr_unsubscribe(ctx->sr_subscription);
}

/* Callback to be called by plugin daemon periodically,
 * to check whether the plugin and managed app is healthy. */
int sr_plugin_health_check_cb(sr_session_ctx_t *session, void *private_ctx)
{
	(void)session;
	(void)private_ctx;
	return SR_ERR_OK;
}


#ifndef PLUGIN
#include <unistd.h>
volatile int exit_application = 0;

static void sigint_handler(int signum)
{
	(void)signum;
	fprintf(stderr, "Sigint called, exiting...\n");
	exit_application = 1;
}

int main(int argc, char *argv[])
{
	sr_conn_ctx_t *connection = NULL;
	sr_session_ctx_t *session = NULL;
	int opt;
	int rc = SR_ERR_OK;
	sr_log_level_t log_level = SR_LL_ERR;
	void *priv = NULL;

	struct option options[] = {
		{"verbosity",		required_argument, NULL, 'v'},
		{NULL,				0,				   NULL, 0},
	};

	/* process options */
	opterr = 0;
	while ((opt = getopt_long(argc, argv, "mv:", options, NULL)) != -1) {
		switch (opt) {
		case 'v':
			if (!strcmp(optarg, "none")) {
				log_level = SR_LL_NONE;
			} else if (!strcmp(optarg, "error")) {
				log_level = SR_LL_ERR;
			} else if (!strcmp(optarg, "warning")) {
				log_level = SR_LL_WRN;
			} else if (!strcmp(optarg, "info")) {
				log_level = SR_LL_INF;
			} else if (!strcmp(optarg, "debug")) {
				log_level = SR_LL_DBG;
			} else if ((strlen(optarg) == 1) && (optarg[0] >= '0') && (optarg[0] <= '4')) {
				log_level = atoi(optarg);
			} else {
				fprintf(stderr, "Invalid verbosity \"%s\"", optarg);
				goto cleanup;
			}
			break;
		default:
			fprintf(stderr, "Invalid option or missing argument: -%c", optopt);
			goto cleanup;
		}
	}

	/* set logging */
	sr_log_stderr(log_level);
	IETF_LOG_DBG("Logging initialized.");

	/* connect to sysrepo */
	rc = sr_connect(SR_CONN_DEFAULT, &connection);
	if (rc != SR_ERR_OK) {
		IETF_LOG_ERR("Error by sr_connect: %s", sr_strerror(rc));
		goto cleanup;
	}
	IETF_LOG_DBG("Connected to sysrepod.");

	/* start session */
	rc = sr_session_start(connection, SR_DS_RUNNING, &session);
	if (rc != SR_ERR_OK) {
		IETF_LOG_ERR("Error by sr_session_start: %s", sr_strerror(rc));
		goto cleanup;
	}
	IETF_LOG_DBG("Sysrepo-session started.");

	/* initialize the plugin */
	rc = sr_plugin_init_cb(session, &priv);
	if (rc != SR_ERR_OK) {
		IETF_LOG_ERR("Error by sr_plugin_init_cb: %s", sr_strerror(rc));
		goto cleanup;
	}
	IETF_LOG_DBG("Plugins initialized.");

	while (!exit_application) {
		IETF_LOG_DBG("Sleeping 1000 ...");
		sleep(1000);  /* or do some more useful work... */
	}

cleanup:
	sr_plugin_cleanup_cb(session, priv);
	IETF_LOG_DBG("Cleanup done.");
}
#endif
