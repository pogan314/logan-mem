/*
 * service_patterns.c — Classify call edges by library identity in resolved QN.
 *
 * Instead of matching callee names (ambiguous: "get", "post", "send"),
 * we match library identifiers in the RESOLVED qualified name. The QN
 * contains the full module path, so import aliases are transparent:
 *   r.get("/api") → QN: project.venv.requests.api.get → match "requests" → HTTP_CALLS
 *
 * Two-level matching:
 *   1. Library identifier in QN → determines edge type (HTTP/ASYNC/CONFIG)
 *   2. Method suffix → determines HTTP method (get→GET, post→POST)
 */
#include "service_patterns.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* ── Library identifier → edge type ────────────────────────────── */

typedef struct {
    const char *library_id; /* substring to find in resolved QN */
    lsm_svc_kind_t kind;    /* HTTP_CALLS, ASYNC_CALLS, CONFIGURES */
    const char *broker;     /* for ASYNC: broker name (NULL otherwise) */
} lib_pattern_t;

/* HTTP client libraries — match these substrings in the resolved QN.
 * Sources: github.com/easybase/awesome-http, official SDK docs, agent research */
static const lib_pattern_t http_libraries[] = {
    /* Python */
    {"requests", LSM_SVC_HTTP, NULL},
    {"httpx", LSM_SVC_HTTP, NULL},
    {"aiohttp", LSM_SVC_HTTP, NULL},
    {"urllib", LSM_SVC_HTTP, NULL},
    {"urllib3", LSM_SVC_HTTP, NULL},
    {"httplib2", LSM_SVC_HTTP, NULL},
    {"pycurl", LSM_SVC_HTTP, NULL},
    {"treq", LSM_SVC_HTTP, NULL},
    {"uplink", LSM_SVC_HTTP, NULL},

    /* JavaScript / TypeScript */
    {"axios", LSM_SVC_HTTP, NULL},
    {"superagent", LSM_SVC_HTTP, NULL},
    {"needle", LSM_SVC_HTTP, NULL},
    {"node-fetch", LSM_SVC_HTTP, NULL},
    {"undici", LSM_SVC_HTTP, NULL},
    {"ofetch", LSM_SVC_HTTP, NULL},
    {"wretch", LSM_SVC_HTTP, NULL},
    {"sindresorhus/ky", LSM_SVC_HTTP, NULL},
    {"phin", LSM_SVC_HTTP, NULL},

    /* Go */
    {"net/http", LSM_SVC_HTTP, NULL},
    {"resty", LSM_SVC_HTTP, NULL},
    {"sling", LSM_SVC_HTTP, NULL},
    {"heimdall", LSM_SVC_HTTP, NULL},
    {"gentleman", LSM_SVC_HTTP, NULL},
    {"retryablehttp", LSM_SVC_HTTP, NULL},

    /* Java / Kotlin */
    {"HttpClient", LSM_SVC_HTTP, NULL},
    {"OkHttp", LSM_SVC_HTTP, NULL},
    {"okhttp3", LSM_SVC_HTTP, NULL},
    {"RestTemplate", LSM_SVC_HTTP, NULL},
    {"WebClient", LSM_SVC_HTTP, NULL},
    {"Unirest", LSM_SVC_HTTP, NULL},
    {"AsyncHttpClient", LSM_SVC_HTTP, NULL},
    {"apache.http", LSM_SVC_HTTP, NULL},
    {"Retrofit", LSM_SVC_HTTP, NULL},
    {"Feign", LSM_SVC_HTTP, NULL},
    {"ktor.client", LSM_SVC_HTTP, NULL},
    {"kittinunf.fuel", LSM_SVC_HTTP, NULL},

    /* Rust */
    {"reqwest", LSM_SVC_HTTP, NULL},
    {"hyper", LSM_SVC_HTTP, NULL},
    {"surf", LSM_SVC_HTTP, NULL},
    {"ureq", LSM_SVC_HTTP, NULL},
    {"isahc", LSM_SVC_HTTP, NULL},
    {"attohttpc", LSM_SVC_HTTP, NULL},

    /* C# */
    {"HttpClient", LSM_SVC_HTTP, NULL},
    {"RestSharp", LSM_SVC_HTTP, NULL},
    {"Flurl", LSM_SVC_HTTP, NULL},
    {"Refit", LSM_SVC_HTTP, NULL},

    /* Ruby */
    {"HTTParty", LSM_SVC_HTTP, NULL},
    {"Faraday", LSM_SVC_HTTP, NULL},
    {"RestClient", LSM_SVC_HTTP, NULL},
    {"Typhoeus", LSM_SVC_HTTP, NULL},
    {"Excon", LSM_SVC_HTTP, NULL},
    {"Net::HTTP", LSM_SVC_HTTP, NULL},

    /* PHP */
    {"Guzzle", LSM_SVC_HTTP, NULL},
    {"guzzle", LSM_SVC_HTTP, NULL},
    {"curl", LSM_SVC_HTTP, NULL},
    {"Symfony\\HttpClient", LSM_SVC_HTTP, NULL},

    /* C/C++ */
    {"cpr", LSM_SVC_HTTP, NULL},
    {"cpp-httplib", LSM_SVC_HTTP, NULL},
    {"Poco.Net", LSM_SVC_HTTP, NULL},
    {"Beast", LSM_SVC_HTTP, NULL},

    /* Swift */
    {"Alamofire", LSM_SVC_HTTP, NULL},
    {"Moya", LSM_SVC_HTTP, NULL},
    {"URLSession", LSM_SVC_HTTP, NULL},

    /* Dart */
    {"Dio", LSM_SVC_HTTP, NULL},
    {"dio", LSM_SVC_HTTP, NULL},
    {"package:http", LSM_SVC_HTTP, NULL},
    {"Chopper", LSM_SVC_HTTP, NULL},

    /* Elixir */
    {"HTTPoison", LSM_SVC_HTTP, NULL},
    {"Tesla", LSM_SVC_HTTP, NULL},
    {"Finch", LSM_SVC_HTTP, NULL},
    {"Mint.HTTP", LSM_SVC_HTTP, NULL},

    /* Scala */
    {"sttp", LSM_SVC_HTTP, NULL},
    {"akka.http", LSM_SVC_HTTP, NULL},
    {"http4s", LSM_SVC_HTTP, NULL},
    {"scalaj", LSM_SVC_HTTP, NULL},

    /* Haskell */
    {"wreq", LSM_SVC_HTTP, NULL},
    {"http-client", LSM_SVC_HTTP, NULL},
    {"http-conduit", LSM_SVC_HTTP, NULL},
    {"servant-client", LSM_SVC_HTTP, NULL},
    {"Network.HTTP", LSM_SVC_HTTP, NULL},

    /* Lua */
    {"socket.http", LSM_SVC_HTTP, NULL},
    {"resty.http", LSM_SVC_HTTP, NULL},

    {NULL, LSM_SVC_NONE, NULL},
};

/* Async dispatch / message broker libraries */
static const lib_pattern_t async_libraries[] = {
    /* GCP */
    {"cloudtasks", LSM_SVC_ASYNC, "cloud_tasks"},
    {"cloud_tasks", LSM_SVC_ASYNC, "cloud_tasks"},
    {"cloud.tasks", LSM_SVC_ASYNC, "cloud_tasks"},
    {"CloudTasks", LSM_SVC_ASYNC, "cloud_tasks"},
    {"pubsub", LSM_SVC_ASYNC, "pubsub"},
    {"cloud.pubsub", LSM_SVC_ASYNC, "pubsub"},
    {"PubSub", LSM_SVC_ASYNC, "pubsub"},

    /* AWS — use SDK module paths to avoid false positives.  lsm_fqn_compute
     * converts path slashes to '.', so a resolved local Go QN reads
     * "aws-sdk-go.service.sqs..."; include both slash and dot forms so the
     * substring match fires whether the id comes from an import path or a QN. */
    {"aws-sdk-go/service/sqs", LSM_SVC_ASYNC, "sqs"},
    {"aws-sdk-go.service.sqs", LSM_SVC_ASYNC, "sqs"},
    {"aws_sdk_sqs", LSM_SVC_ASYNC, "sqs"},
    {"Amazon.SQS", LSM_SVC_ASYNC, "sqs"},
    {"@aws-sdk/client-sqs", LSM_SVC_ASYNC, "sqs"},
    {"boto3.client.sqs", LSM_SVC_ASYNC, "sqs"},
    {"aws-sdk-go/service/sns", LSM_SVC_ASYNC, "sns"},
    {"aws-sdk-go.service.sns", LSM_SVC_ASYNC, "sns"},
    {"aws_sdk_sns", LSM_SVC_ASYNC, "sns"},
    {"Amazon.SNS", LSM_SVC_ASYNC, "sns"},
    {"@aws-sdk/client-sns", LSM_SVC_ASYNC, "sns"},
    {"eventbridge", LSM_SVC_ASYNC, "eventbridge"},
    {"EventBridge", LSM_SVC_ASYNC, "eventbridge"},
    {"aws-sdk-go/service/lambda", LSM_SVC_ASYNC, "lambda"},
    {"aws-sdk-go.service.lambda", LSM_SVC_ASYNC, "lambda"},
    {"aws_sdk_lambda", LSM_SVC_ASYNC, "lambda"},
    {"@aws-sdk/client-lambda", LSM_SVC_ASYNC, "lambda"},
    {"stepfunctions", LSM_SVC_ASYNC, "stepfunctions"},

    /* Azure */
    {"ServiceBus", LSM_SVC_ASYNC, "servicebus"},
    {"Azure.Messaging", LSM_SVC_ASYNC, "servicebus"},

    /* Kafka */
    {"kafka", LSM_SVC_ASYNC, "kafka"},
    {"Kafka", LSM_SVC_ASYNC, "kafka"},
    {"kafkajs", LSM_SVC_ASYNC, "kafka"},
    {"sarama", LSM_SVC_ASYNC, "kafka"},
    {"rdkafka", LSM_SVC_ASYNC, "kafka"},
    {"confluent", LSM_SVC_ASYNC, "kafka"},
    {"Confluent.Kafka", LSM_SVC_ASYNC, "kafka"},

    /* RabbitMQ */
    {"amqp", LSM_SVC_ASYNC, "rabbitmq"},
    {"AMQP", LSM_SVC_ASYNC, "rabbitmq"},
    {"amqplib", LSM_SVC_ASYNC, "rabbitmq"},
    {"RabbitMQ", LSM_SVC_ASYNC, "rabbitmq"},
    {"lapin", LSM_SVC_ASYNC, "rabbitmq"},
    {"MassTransit", LSM_SVC_ASYNC, "rabbitmq"},

    /* NATS */
    {"nats", LSM_SVC_ASYNC, "nats"},
    {"NATS", LSM_SVC_ASYNC, "nats"},

    /* Redis pub/sub */
    {"ioredis", LSM_SVC_ASYNC, "redis"},

    /* Task queues */
    {"celery", LSM_SVC_ASYNC, "celery"},
    {"Celery", LSM_SVC_ASYNC, "celery"},
    {"dramatiq", LSM_SVC_ASYNC, "dramatiq"},
    {"huey", LSM_SVC_ASYNC, "huey"},
    {"python-rq", LSM_SVC_ASYNC, "rq"},
    {"rq.Queue", LSM_SVC_ASYNC, "rq"},
    {"bullmq", LSM_SVC_ASYNC, "bullmq"},
    {"BullMQ", LSM_SVC_ASYNC, "bullmq"},
    {"bull.Queue", LSM_SVC_ASYNC, "bull"},
    {"Sidekiq", LSM_SVC_ASYNC, "sidekiq"},
    {"sidekiq", LSM_SVC_ASYNC, "sidekiq"},
    {"Resque", LSM_SVC_ASYNC, "resque"},
    {"GoodJob", LSM_SVC_ASYNC, "goodjob"},
    {"DelayedJob", LSM_SVC_ASYNC, "delayed_job"},
    {"Hangfire", LSM_SVC_ASYNC, "hangfire"},
    {"NServiceBus", LSM_SVC_ASYNC, "nservicebus"},
    {"asynq", LSM_SVC_ASYNC, "asynq"},
    {"RichardKnop/machinery", LSM_SVC_ASYNC, "machinery"},

    /* Workflow engines — use specific module paths to avoid "Temporal" in Django etc. */
    {"temporalio", LSM_SVC_ASYNC, "temporal"},
    {"@temporalio", LSM_SVC_ASYNC, "temporal"},
    {"temporal.client", LSM_SVC_ASYNC, "temporal"},
    {"temporal.worker", LSM_SVC_ASYNC, "temporal"},
    {"inngest", LSM_SVC_ASYNC, "inngest"},

    /* Elixir */
    {"Oban", LSM_SVC_ASYNC, "oban"},
    {"Broadway", LSM_SVC_ASYNC, "broadway"},
    {"GenStage", LSM_SVC_ASYNC, "genstage"},
    {"Phoenix.PubSub", LSM_SVC_ASYNC, "phoenix_pubsub"},

    /* Scala */
    {"Alpakka", LSM_SVC_ASYNC, "alpakka"},

    /* MQTT */
    {"mqtt", LSM_SVC_ASYNC, "mqtt"},
    {"paho.mqtt", LSM_SVC_ASYNC, "mqtt"},
    {"MQTTClient", LSM_SVC_ASYNC, "mqtt"},
    {"mosquitto", LSM_SVC_ASYNC, "mqtt"},
    {"asyncio_mqtt", LSM_SVC_ASYNC, "mqtt"},
    {"gmqtt", LSM_SVC_ASYNC, "mqtt"},
    {"rumqttc", LSM_SVC_ASYNC, "mqtt"},

    /* NATS */
    {"nats.go", LSM_SVC_ASYNC, "nats"},
    {"nats-py", LSM_SVC_ASYNC, "nats"},
    {"nats.ws", LSM_SVC_ASYNC, "nats"},
    {"nats.java", LSM_SVC_ASYNC, "nats"},
    {"nats.net", LSM_SVC_ASYNC, "nats"},
    {"async-nats", LSM_SVC_ASYNC, "nats"},
    {"nats.rs", LSM_SVC_ASYNC, "nats"},

    /* Dapr pub/sub */
    {"dapr.clients.grpc", LSM_SVC_ASYNC, "dapr"},
    {"DaprClient", LSM_SVC_ASYNC, "dapr"},

    {NULL, LSM_SVC_NONE, NULL},
};

/* Config accessor libraries */
static const lib_pattern_t config_libraries[] = {
    /* Universal */
    {"getenv", LSM_SVC_CONFIG, NULL},
    {"Getenv", LSM_SVC_CONFIG, NULL},
    {"getEnv", LSM_SVC_CONFIG, NULL},
    {"LookupEnv", LSM_SVC_CONFIG, NULL},
    {"lookupEnv", LSM_SVC_CONFIG, NULL},
    {"get_env", LSM_SVC_CONFIG, NULL},
    {"fetch_env", LSM_SVC_CONFIG, NULL},
    {"GetEnvironmentVariable", LSM_SVC_CONFIG, NULL},
    {"getProperty", LSM_SVC_CONFIG, NULL},
    {"getEnvironment", LSM_SVC_CONFIG, NULL},

    /* Go */
    {"viper", LSM_SVC_CONFIG, NULL},
    {"envconfig", LSM_SVC_CONFIG, NULL},
    {"godotenv", LSM_SVC_CONFIG, NULL},

    /* Python */
    {"decouple", LSM_SVC_CONFIG, NULL},
    {"dynaconf", LSM_SVC_CONFIG, NULL},
    {"dotenv", LSM_SVC_CONFIG, NULL},

    /* JS/TS */
    {"nconf", LSM_SVC_CONFIG, NULL},
    {"convict", LSM_SVC_CONFIG, NULL},
    {"envalid", LSM_SVC_CONFIG, NULL},

    /* Rust */
    {"dotenvy", LSM_SVC_CONFIG, NULL},
    {"figment", LSM_SVC_CONFIG, NULL},
    {"config-rs", LSM_SVC_CONFIG, NULL},

    /* Java/Scala */
    {"ConfigFactory", LSM_SVC_CONFIG, NULL},
    {"ConfigurationProperties", LSM_SVC_CONFIG, NULL},

    /* Elixir */
    {"Application.get_env", LSM_SVC_CONFIG, NULL},
    {"Application.fetch_env", LSM_SVC_CONFIG, NULL},

    {NULL, LSM_SVC_NONE, NULL},
};

/* Route registration frameworks — callee resolves to one of these AND
 * has an HTTP method suffix → LSM_SVC_ROUTE_REG.
 * Distinguished from HTTP clients: "gin.GET" registers a handler,
 * "requests.get" makes an outbound HTTP call. */
static const lib_pattern_t route_reg_libraries[] = {
    /* Go */
    {"gin-gonic/gin", LSM_SVC_ROUTE_REG, NULL},
    {"gin.", LSM_SVC_ROUTE_REG, NULL},
    {"go-chi/chi", LSM_SVC_ROUTE_REG, NULL},
    {"chi.", LSM_SVC_ROUTE_REG, NULL},
    {"gorilla/mux", LSM_SVC_ROUTE_REG, NULL},
    {"labstack/echo", LSM_SVC_ROUTE_REG, NULL},
    {"echo.", LSM_SVC_ROUTE_REG, NULL},
    {"gofiber/fiber", LSM_SVC_ROUTE_REG, NULL},
    {"fiber.", LSM_SVC_ROUTE_REG, NULL},
    {"net/http.ServeMux", LSM_SVC_ROUTE_REG, NULL},
    {"http.ServeMux", LSM_SVC_ROUTE_REG, NULL},
    {"httprouter", LSM_SVC_ROUTE_REG, NULL},

    /* JavaScript / TypeScript */
    {"express", LSM_SVC_ROUTE_REG, NULL},
    {"fastify", LSM_SVC_ROUTE_REG, NULL},
    {"koa-router", LSM_SVC_ROUTE_REG, NULL},
    {"hono", LSM_SVC_ROUTE_REG, NULL},
    {"hapi", LSM_SVC_ROUTE_REG, NULL},

    /* Python (non-decorator, e.g., Flask add_url_rule) */
    {"flask", LSM_SVC_ROUTE_REG, NULL},
    {"FastAPI", LSM_SVC_ROUTE_REG, NULL},
    {"starlette", LSM_SVC_ROUTE_REG, NULL},

    /* PHP */
    {"Laravel", LSM_SVC_ROUTE_REG, NULL},
    {"Illuminate.Routing", LSM_SVC_ROUTE_REG, NULL},
    {"Symfony.Routing", LSM_SVC_ROUTE_REG, NULL},

    /* Kotlin */
    {"ktor.server", LSM_SVC_ROUTE_REG, NULL},
    {"ktor.routing", LSM_SVC_ROUTE_REG, NULL},

    /* Rust */
    {"actix-web", LSM_SVC_ROUTE_REG, NULL},
    {"actix_web", LSM_SVC_ROUTE_REG, NULL},
    {"axum", LSM_SVC_ROUTE_REG, NULL},
    {"rocket", LSM_SVC_ROUTE_REG, NULL},

    /* Java */
    {"Spring", LSM_SVC_ROUTE_REG, NULL},
    {"jakarta.ws.rs", LSM_SVC_ROUTE_REG, NULL},

    /* C# */
    {"Microsoft.AspNetCore", LSM_SVC_ROUTE_REG, NULL},
    {"MapGet", LSM_SVC_ROUTE_REG, NULL},
    {"MapPost", LSM_SVC_ROUTE_REG, NULL},

    /* Ruby */
    {"ActionDispatch", LSM_SVC_ROUTE_REG, NULL},
    {"Sinatra", LSM_SVC_ROUTE_REG, NULL},

    /* Elixir */
    {"Phoenix.Router", LSM_SVC_ROUTE_REG, NULL},

    /* Scala */
    {"akka.http.scaladsl.server", LSM_SVC_ROUTE_REG, NULL},
    {"play.api.routing", LSM_SVC_ROUTE_REG, NULL},

    {NULL, LSM_SVC_NONE, NULL},
};

/* gRPC client libraries — protobuf stub invocations */
static const lib_pattern_t grpc_libraries[] = {
    /* Go */
    {"google.golang.org/grpc", LSM_SVC_GRPC, NULL},
    {"grpc.Dial", LSM_SVC_GRPC, NULL},
    {"grpc.NewClient", LSM_SVC_GRPC, NULL},
    {"grpc.DialContext", LSM_SVC_GRPC, NULL},

    /* Python */
    {"grpc.insecure_channel", LSM_SVC_GRPC, NULL},
    {"grpc.secure_channel", LSM_SVC_GRPC, NULL},
    {"grpcio", LSM_SVC_GRPC, NULL},
    {"grpc.aio", LSM_SVC_GRPC, NULL},

    /* Java/Kotlin */
    {"io.grpc", LSM_SVC_GRPC, NULL},
    {"ManagedChannelBuilder", LSM_SVC_GRPC, NULL},
    {"ManagedChannel", LSM_SVC_GRPC, NULL},
    {"newBlockingStub", LSM_SVC_GRPC, NULL},
    {"newFutureStub", LSM_SVC_GRPC, NULL},

    /* C# */
    {"Grpc.Net.Client", LSM_SVC_GRPC, NULL},
    {"GrpcChannel", LSM_SVC_GRPC, NULL},
    {"Grpc.Core", LSM_SVC_GRPC, NULL},

    /* JS/TS */
    {"@grpc/grpc-js", LSM_SVC_GRPC, NULL},
    {"grpc-web", LSM_SVC_GRPC, NULL},

    /* Rust */
    {"tonic", LSM_SVC_GRPC, NULL},

    /* Dart/Flutter */
    {"package:grpc", LSM_SVC_GRPC, NULL},

    {NULL, LSM_SVC_NONE, NULL},
};

/* GraphQL client libraries */
static const lib_pattern_t graphql_libraries[] = {
    /* JS/TS */
    {"graphql-request", LSM_SVC_GRAPHQL, NULL},
    {"@apollo/client", LSM_SVC_GRAPHQL, NULL},
    {"apollo-client", LSM_SVC_GRAPHQL, NULL},
    {"urql", LSM_SVC_GRAPHQL, NULL},
    {"graphql-tag", LSM_SVC_GRAPHQL, NULL},

    /* Python */
    {"gql", LSM_SVC_GRAPHQL, NULL},
    {"sgqlc", LSM_SVC_GRAPHQL, NULL},
    {"graphene", LSM_SVC_GRAPHQL, NULL},

    /* Java */
    {"graphql-java", LSM_SVC_GRAPHQL, NULL},
    {"DgsQueryExecutor", LSM_SVC_GRAPHQL, NULL},

    /* Go */
    {"graphql-go", LSM_SVC_GRAPHQL, NULL},
    {"gqlgen", LSM_SVC_GRAPHQL, NULL},

    /* Ruby */
    {"graphql-ruby", LSM_SVC_GRAPHQL, NULL},

    /* Rust */
    {"async-graphql", LSM_SVC_GRAPHQL, NULL},
    {"juniper", LSM_SVC_GRAPHQL, NULL},

    {NULL, LSM_SVC_NONE, NULL},
};

/* tRPC libraries (TypeScript only) */
static const lib_pattern_t trpc_libraries[] = {
    {"@trpc/server", LSM_SVC_TRPC, NULL},
    {"@trpc/client", LSM_SVC_TRPC, NULL},
    {"@trpc/react-query", LSM_SVC_TRPC, NULL},
    {"createTRPCRouter", LSM_SVC_TRPC, NULL},
    {"createTRPCProxyClient", LSM_SVC_TRPC, NULL},

    {NULL, LSM_SVC_NONE, NULL},
};

/* Method suffix type (used by both route registration and HTTP client tables) */
typedef struct {
    const char *suffix;
    const char *method;
} method_suffix_t;

/* Route registration method suffixes — matched on callee name.
 * These are methods on router objects that register handlers. */
static const method_suffix_t route_reg_suffixes[] = {
    /* HTTP method registrations */
    {".GET", "GET"},
    {".Get", "GET"},
    {".get", "GET"},
    {".POST", "POST"},
    {".Post", "POST"},
    {".post", "POST"},
    {".PUT", "PUT"},
    {".Put", "PUT"},
    {".put", "PUT"},
    {".DELETE", "DELETE"},
    {".Delete", "DELETE"},
    {".delete", "DELETE"},
    {".PATCH", "PATCH"},
    {".Patch", "PATCH"},
    {".patch", "PATCH"},
    /* Handle/HandleFunc (Go stdlib, gorilla) */
    {".Handle", "ANY"},
    {".HandleFunc", "ANY"},
    {".handle", "ANY"},
    /* Framework-specific route registration */
    {".Route", "ANY"},
    {".route", "ANY"},
    {"::get", "GET"},
    {"::post", "POST"},
    {"::put", "PUT"},
    {"::delete", "DELETE"},
    {"::patch", "PATCH"},
    /* Minimal API (C# ASP.NET) */
    {".MapGet", "GET"},
    {".MapPost", "POST"},
    {".MapPut", "PUT"},
    {".MapDelete", "DELETE"},
    /* Router mounting / prefix registration (any method) */
    {".include_router", "ANY"},
    {".mount", "ANY"},
    {".add_url_rule", "ANY"},
    {".register_blueprint", "ANY"},
    {".use", "ANY"},
    {".register", "ANY"},
    {".add_route", "ANY"},
    {".add_api_route", "ANY"},
    {".add_api_websocket_route", "ANY"},
    {NULL, NULL},
};

/* ── HTTP method inference from function/method name suffix ───── */

static const method_suffix_t method_suffixes[] = {
    {".get", "GET"},           {".Get", "GET"},           {".GET", "GET"},
    {".post", "POST"},         {".Post", "POST"},         {".POST", "POST"},
    {".put", "PUT"},           {".Put", "PUT"},           {".PUT", "PUT"},
    {".delete", "DELETE"},     {".Delete", "DELETE"},     {".DELETE", "DELETE"},
    {".patch", "PATCH"},       {".Patch", "PATCH"},       {".PATCH", "PATCH"},
    {".head", "HEAD"},         {".Head", "HEAD"},         {".HEAD", "HEAD"},
    {".options", "OPTIONS"},   {".Options", "OPTIONS"},   {"GetAsync", "GET"},
    {"PostAsync", "POST"},     {"PutAsync", "PUT"},       {"DeleteAsync", "DELETE"},
    {"SendAsync", NULL},       {"getForObject", "GET"},   {"getForEntity", "GET"},
    {"postForObject", "POST"}, {"postForEntity", "POST"}, {NULL, NULL},
};

/* ── Matching implementation ───────────────────────────────────── */

/* Check if any library identifier appears as a substring in the QN.
 * Case-sensitive: "requests" matches "project.venv.requests.api.get"
 * but not "Requests". Library names are specific enough to avoid
 * false positives even with substring matching. */
static const lib_pattern_t *match_qn(const char *qn, const lib_pattern_t *patterns) {
    if (!qn || !qn[0]) {
        return NULL;
    }
    for (int i = 0; patterns[i].library_id != NULL; i++) {
        if (strstr(qn, patterns[i].library_id) != NULL) {
            return &patterns[i];
        }
    }
    return NULL;
}

static bool starts_with_segment(const char *path, const char *segment) {
    if (!path || path[0] != '/' || !segment) {
        return false;
    }
    size_t seg_len = strlen(segment);
    const char *p = path + 1;
    return strncmp(p, segment, seg_len) == 0 && (p[seg_len] == '\0' || p[seg_len] == '/');
}

static bool contains_segment(const char *path, const char *segment) {
    if (!path || !segment) {
        return false;
    }
    size_t seg_len = strlen(segment);
    const char *p = path;
    while ((p = strchr(p, '/')) != NULL) {
        p++;
        if (strncmp(p, segment, seg_len) == 0 && (p[seg_len] == '\0' || p[seg_len] == '/')) {
            return true;
        }
    }
    return false;
}

static bool is_digit_char(char ch) {
    return ch >= '0' && ch <= '9';
}

static bool has_http_route_marker(const char *path) {
    if (starts_with_segment(path, "api") || starts_with_segment(path, "apis") ||
        starts_with_segment(path, "graphql") || starts_with_segment(path, "health") ||
        starts_with_segment(path, "metrics")) {
        return true;
    }
    return path && path[0] == '/' && path[1] == 'v' && is_digit_char(path[2]) &&
           (path[3] == '\0' || path[3] == '/');
}

static bool has_filesystem_root(const char *path) {
    static const char *const roots[] = {"etc",     "root", "var",   "usr",     "home", "tmp",
                                        "private", "opt",  "bin",   "sbin",    "dev",  "proc",
                                        "sys",     "run",  "lib",   "lib64",   "mnt",  "media",
                                        "boot",    "srv",  "Users", "Volumes", NULL};
    for (int i = 0; roots[i]; i++) {
        if (starts_with_segment(path, roots[i])) {
            return true;
        }
    }
    return false;
}

static bool has_hidden_config_segment(const char *path) {
    static const char *const segments[] = {".aws", ".azure", ".config", ".docker", ".env",
                                           ".git", ".gnupg", ".kube",   ".ssh",    NULL};
    for (int i = 0; segments[i]; i++) {
        if (contains_segment(path, segments[i])) {
            return true;
        }
    }
    return false;
}

static bool path_ext_matches(const char *ext, const char *wanted) {
    return ext && wanted && strcmp(ext, wanted) == 0;
}

static bool has_filesystem_extension(const char *path) {
    if (!path) {
        return false;
    }
    const char *end = strpbrk(path, "?#");
    if (!end) {
        end = path + strlen(path);
    }
    const char *last_slash = path;
    for (const char *p = path; p < end; p++) {
        if (*p == '/') {
            last_slash = p;
        }
    }
    const char *dot = NULL;
    for (const char *p = last_slash + 1; p < end; p++) {
        if (*p == '.') {
            dot = p;
        }
    }
    if (!dot || dot == end - 1) {
        return false;
    }
    char ext[32];
    size_t ext_len = (size_t)(end - dot);
    if (ext_len >= sizeof(ext)) {
        return false;
    }
    memcpy(ext, dot, ext_len);
    ext[ext_len] = '\0';

    static const char *const hard_file_exts[] = {
        ".cfg",  ".conf",   ".credentials", ".crt",  ".db",         ".env",
        ".ini",  ".key",    ".pem",         ".pid",  ".properties", ".service",
        ".sock", ".socket", ".sqlite",      ".toml", NULL};
    for (int i = 0; hard_file_exts[i]; i++) {
        if (path_ext_matches(ext, hard_file_exts[i])) {
            return true;
        }
    }
    if ((path_ext_matches(ext, ".json") || path_ext_matches(ext, ".yaml") ||
         path_ext_matches(ext, ".yml") || path_ext_matches(ext, ".xml")) &&
        !has_http_route_marker(path)) {
        return true;
    }
    return false;
}

static bool callee_is_delimiter_or_filesystem_builder(const char *callee_name) {
    if (!callee_name) {
        return false;
    }
    const char *last_dot = strrchr(callee_name, '.');
    const char *last_colon = strstr(callee_name, "::");
    const char *method = callee_name;
    if (last_dot && last_dot[1]) {
        method = last_dot + 1;
    }
    if (last_colon && last_colon[2]) {
        method = last_colon + 2;
    }
    if (strcmp(method, "split") == 0 || strcmp(method, "rsplit") == 0 ||
        strcmp(method, "partition") == 0 || strcmp(method, "join") == 0) {
        return true;
    }
    return strstr(callee_name, "os.path.join") != NULL || strstr(callee_name, "path.join") != NULL;
}

static const char *strip_string_delimiters(const char *literal, char *buf, size_t buf_sz) {
    if (!literal || !literal[0]) {
        return NULL;
    }
    const char *start = literal;
    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r') {
        start++;
    }
    size_t len = strlen(start);
    while (len > 0 && (start[len - 1] == ' ' || start[len - 1] == '\t' || start[len - 1] == '\n' ||
                       start[len - 1] == '\r')) {
        len--;
    }
    if (len >= 2 && (start[0] == '"' || start[0] == '\'' || start[0] == '`') &&
        start[len - 1] == start[0]) {
        start++;
        len -= 2;
    }
    if (len == 0 || len >= buf_sz) {
        return NULL;
    }
    memcpy(buf, start, len);
    buf[len] = '\0';
    return buf;
}

bool lsm_service_pattern_is_http_route_literal(const char *literal, const char *callee_name) {
    char path_buf[1024];
    const char *path = strip_string_delimiters(literal, path_buf, sizeof(path_buf));
    if (!path || !path[0]) {
        return false;
    }
    if (strncmp(path, "http://", 7) == 0 || strncmp(path, "https://", 8) == 0) {
        return true;
    }
    if (strstr(path, "://") != NULL) {
        return false;
    }
    if (path[0] != '/') {
        return false;
    }
    if (callee_is_delimiter_or_filesystem_builder(callee_name)) {
        return false;
    }
    if (has_filesystem_root(path) || has_hidden_config_segment(path) ||
        has_filesystem_extension(path)) {
        return false;
    }
    return true;
}

/* ── Public API ────────────────────────────────────────────────── */

/* Per-worker TLS cache of lsm_service_pattern_match results.
 * The hot path in resolve_file_calls invokes pattern matching for
 * EVERY resolved CALL (via emit_service_edge) — that's 6 pattern-list
 * scans × ~30 patterns × strstr per call. On kubernetes (~600k
 * resolved call edges), the same resolved QN (e.g. "context.Context.
 * Done", "fmt.Errorf", "errors.New") repeats hundreds of thousands of
 * times. A simple TLS hash cache turns the linear scan into one
 * lookup after the first miss for that QN. Lifetime is per-worker for
 * the duration of the parallel_resolve phase. */
#include "foundation/hash_table.h"
#include "foundation/compat.h"

static LSM_TLS LSMHashTable *_svc_cache = NULL;
/* Encode the enum + 1 in the pointer so 0/NULL means "miss". */
static inline void *svc_enum_to_ptr(lsm_svc_kind_t k) {
    return (void *)(uintptr_t)((unsigned)k + 1u);
}
static inline lsm_svc_kind_t svc_ptr_to_enum(void *p) {
    return (lsm_svc_kind_t)((uintptr_t)p - 1u);
}

static void svc_cache_free_key(const char *key, void *val, void *ud) {
    (void)val;
    (void)ud;
    free((char *)key);
}

void lsm_service_pattern_cache_begin(void) {
    if (_svc_cache)
        return; /* idempotent */
    _svc_cache = lsm_ht_create(8192);
}

void lsm_service_pattern_cache_end(void) {
    if (!_svc_cache)
        return;
    lsm_ht_foreach(_svc_cache, svc_cache_free_key, NULL);
    lsm_ht_free(_svc_cache);
    _svc_cache = NULL;
}

void lsm_service_patterns_init(void) {
    /* No-op — tables are static const */
}

bool lsm_service_pattern_is_global_fetch(const char *callee_name) {
    return callee_name != NULL && strcmp(callee_name, "fetch") == 0;
}

lsm_svc_kind_t lsm_service_pattern_match(const char *resolved_qn) {
    if (!resolved_qn || !resolved_qn[0]) {
        return LSM_SVC_NONE;
    }

    if (_svc_cache) {
        void *cached = lsm_ht_get(_svc_cache, resolved_qn);
        if (cached) {
            return svc_ptr_to_enum(cached);
        }
    }

    lsm_svc_kind_t result = LSM_SVC_NONE;
    const lib_pattern_t *p;

    /* Route registration checked first — prevents gin/echo from matching
     * as HTTP clients (both have .get/.post suffixes). */
    if ((p = match_qn(resolved_qn, route_reg_libraries)))
        result = p->kind;
    else if ((p = match_qn(resolved_qn, http_libraries)))
        result = p->kind;
    else if ((p = match_qn(resolved_qn, async_libraries)))
        result = p->kind;
    else if ((p = match_qn(resolved_qn, config_libraries)))
        result = p->kind;
    else if ((p = match_qn(resolved_qn, grpc_libraries)))
        result = p->kind;
    else if ((p = match_qn(resolved_qn, graphql_libraries)))
        result = p->kind;
    else if ((p = match_qn(resolved_qn, trpc_libraries)))
        result = p->kind;

    if (_svc_cache) {
        char *kdup = strdup(resolved_qn);
        if (kdup)
            lsm_ht_set(_svc_cache, kdup, svc_enum_to_ptr(result));
    }
    return result;
}

const char *lsm_service_pattern_http_method(const char *callee_name) {
    if (!callee_name) {
        return NULL;
    }
    for (int i = 0; method_suffixes[i].suffix != NULL; i++) {
        size_t slen = strlen(method_suffixes[i].suffix);
        size_t clen = strlen(callee_name);
        if (clen >= slen && strcmp(callee_name + clen - slen, method_suffixes[i].suffix) == 0) {
            return method_suffixes[i].method;
        }
    }
    return NULL;
}

const char *lsm_service_pattern_route_method(const char *callee_name) {
    if (!callee_name) {
        return NULL;
    }
    size_t clen = strlen(callee_name);
    for (int i = 0; route_reg_suffixes[i].suffix != NULL; i++) {
        size_t slen = strlen(route_reg_suffixes[i].suffix);
        if (clen >= slen && strcmp(callee_name + clen - slen, route_reg_suffixes[i].suffix) == 0) {
            return route_reg_suffixes[i].method;
        }
    }
    return NULL;
}

const char *lsm_service_pattern_broker(const char *resolved_qn) {
    if (!resolved_qn) {
        return NULL;
    }
    const lib_pattern_t *p = match_qn(resolved_qn, async_libraries);
    return p ? p->broker : NULL;
}
