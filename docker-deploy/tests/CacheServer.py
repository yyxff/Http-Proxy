from flask import Flask, request, jsonify, Response, make_response

# create app
app = Flask(__name__)
disabled_paths = set()

# enroll before request hook
@app.before_request
def check_disabled_routes():
    if request.path in disabled_paths:
        return jsonify({"error": "this path has been disabled"}), 403
    
# root
@app.route('/')
def home():
    return "welcome to simple test server!"


# test1: valid cache
@app.route('/valid-cache')
def cache():
    disabled_paths.add('/valid-cache')
    return f"hello! I'm valid_cache!"


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


# start server
if __name__ == '__main__':
    app.run(host='127.0.0.1', port=5000, debug=True)
