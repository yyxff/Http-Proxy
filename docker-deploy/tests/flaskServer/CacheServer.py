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
    # disable this path after first visit
    disabled_paths.add('/valid-cache')
    return response


# test2: revalid cache
myEtag = "v1"
visited_time = 0
@app.route('/revalid-cache')
def revalid_cache():
    global visited_time, myEtag
    visited_time += 1
    if visited_time > 2:
        myEtag = "v2"
    response = make_response("hello! I'm revalid_cache!")
    response.set_etag(myEtag)
    
    if response.make_conditional(request):
        return response

    return response

# test3 max-age cache
@app.route('/cache/max-age')
def max_age_cache():
    response = make_response("hello! I'm max_age_cache!")
    response.headers['Cache-Control'] = 'public, max-age=3'

    return response

@app.route('/cache/no-store')
def cache_no_store():
    global message_no_store
    response = make_response("hello! I'm no-store!")
    response.headers['Cache-Control'] = 'no-store'
    response.headers['message'] = message_no_store
    message_no_store = "v2"  # update message, so next request can verify if a new response is obtained
    return response

@app.route('/cache/min-fresh')
def cache_min_fresh():
    response = make_response("hello! I'm min_fresh_cache!")
    response.headers['Cache-Control'] = 'public, max-age=5'
    response.headers['X-Timestamp'] = str(time.time())
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
    response.headers['Cache-Control'] = 'public, max-age=2'
    response.headers['message'] = message_max_stale
    message_max_stale = "v2"
    return response

# start server
if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)
