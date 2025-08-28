from flask import Flask, request, jsonify, Response, make_response
import time

# create app
app = Flask(__name__)
disabled_paths = set()
message_no_store = "v1"

# add reset function
@app.route('/reset')
def reset():
    global disabled_paths, message_no_store
    disabled_paths.clear()
    message_no_store = "v1"
    return "Server state reset"

# enroll before request hook
@app.before_request
def check_disabled_routes():
    if request.path in disabled_paths:
        return jsonify({"error": "this path has been disabled"}), 403
    
# root
@app.route('/')
def home():
    global disabled_paths, message_no_store
    disabled_paths.clear()  # reset state when visiting root path
    message_no_store = "v1"  # reset message_no_store variable
    return "welcome to simple test server!"


# test1: valid cache
@app.route('/valid-cache')
def cache():
    response = make_response("hello! I'm valid_cache!")
    response.headers['Cache-Control'] = 'max-age=60'  # set cache time to 60 seconds
    response.headers['message'] = 'v1'
    # disable this path after first visit
    disabled_paths.add('/valid-cache')
    return response


# test2: revalid cache
myEtag = "v1"
message_revalid = "v1"
visited_time = 0
@app.route('/revalid-cache')
def revalid_cache():
    global visited_time, myEtag, message_revalid
    visited_time += 1
    if visited_time > 2:
        myEtag = "v2"
        message_revalid = "v2"
    response = make_response("hello! I'm revalid_cache!")
    response.set_etag(myEtag)
    response.headers['message'] = message_revalid 
    
    if response.make_conditional(request):
        return response

    return response

# test only if cached cache
message_only_cached = "v1"
@app.route('/cache/only-if-cached')
def only_if_cached():
    global message_only_cached
    response = make_response("hello! I'm only_cached!")
    response.headers['Cache-Control'] = 'public, max-age=3'
    response.headers['message'] = message_only_cached
    message_only_cached = "v2"
    return response

# test3 max-age cache
message_max_age = "v1"
@app.route('/cache/max-age')
def max_age_cache():
    global message_max_age
    response = make_response("hello! I'm max_age_cache!")
    response.headers['Cache-Control'] = 'public, max-age=3'
    response.headers['message'] = message_max_age
    message_max_age = "v2"
    return response

@app.route('/cache/no-store')
def cache_no_store():
    global message_no_store
    response = make_response("hello! I'm no-store!")
    response.headers['Cache-Control'] = 'no-store, max-age=10'
    response.headers['message'] = message_no_store
    message_no_store = "v2"  # update message, so next request can verify if a new response is obtained
    return response

message_min_fresh = "v1"
@app.route('/cache/min-fresh')
def cache_min_fresh():
    global message_min_fresh
    response = make_response("hello! I'm min_fresh_cache!")
    response.headers['Cache-Control'] = 'max-age=5'
    response.headers['message'] = message_min_fresh
    response.headers['X-Timestamp'] = str(time.time())
    message_min_fresh = "v2"
    return response

@app.route('/swarm')
def swarm():
    return "welcome to simple swarm server!"

# test 18 max-stale
# first return v1
# after return v2
message_max_stale = "v1"
@app.route('/cache/max-stale')
def cache_max_stale():
    global message_max_stale
    response = make_response("hello! I'm max-stale!")
    response.headers['Cache-Control'] = 'public, max-age=0'
    response.headers['message'] = message_max_stale
    message_max_stale = "v2"
    return response

# test for response no-cache directive
message_no_cache = "v1"
@app.route('/cache/response-no-cache')
def cache_response_no_cache():
    global message_no_cache
    response = make_response("hello! I'm response-no-cache!")
    response.headers['Cache-Control'] = 'no-cache'
    response.headers['message'] = message_no_cache
    message_no_cache = "v2"
    return response

# test for response must-revalidate directive
message_must_revalidate = "v1"
etag_must_revalidate = "etag1"
@app.route('/cache/must-revalidate')
def cache_must_revalidate():
    global message_must_revalidate, etag_must_revalidate
    response = make_response("hello! I'm must-revalidate!")
    response.headers['Cache-Control'] = 'public, max-age=2, must-revalidate'
    response.headers['message'] = message_must_revalidate
    response.set_etag(etag_must_revalidate)
    
    if request.headers.get('If-None-Match') == etag_must_revalidate:
        return response, 304
    etag_must_revalidate = "etag2"
    message_must_revalidate = "v2"
    return response


# test for response no-cache directive
message_no_cache_res = "v1"
@app.route('/cache/response-no-store')
def cache_response_no_tore():
    global message_no_cache_res
    response = make_response("hello! I'm response-no-store!")
    response.headers['Cache-Control'] = 'no-store, max-age=30'
    response.headers['message'] = message_no_cache_res
    message_no_cache_res = "v2"
    return response

# start server
if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5001, debug=True)
