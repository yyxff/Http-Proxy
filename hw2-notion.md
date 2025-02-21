# Getting started

Fair warning at the start -- you will need progress during the full duration of the assignment and will greatly benefit from working at a steady pace.  Like all assignments in this class, this is certainly not easy.  It's expected that you will run into things that you're not immediately sure how to implement.  But with some thought, you'll be able to come up with good solutions and learn valuable programming skills!  This assignment will give you experience with several new things, such as daemons, network programming, coding to a protocol, and caching (i.e. you'll find this very rewarding at the end if you give yourself time to do a good job).

Find your new partner for HW2 as soon as possible, and go ahead and get started sketching out a plan & high-level design. Please feel free to reach out to instructor(s) for “automatic” partner pairing.  Note also that (as we discussed for daemons) you will be dealing with multi-threaded code.  Debugging can be difficult so you'll want to leave time for working through issues.

## **Important Notes**

- Use of good object-oriented C++ abstractions and RAII (something we'll discuss in class soon, related to exceptions) is highly encouraged as it will make your coding go much smoother.
- Your code should be developed only by you and your partner. Using libraries (e.g. Boost), consulting resources about how sockets work, consulting resources about the HTTP protocol is good. But what would not be acceptable would be to use other's code or search and use things from a resource like "here is code for a proxy server" or "So you have an assignment to write a web proxy? Heres what you do..." If you're not sure whether a library is reasonable to use, please ask.
- You can use libraries to help with http parsing (i.e. help you get the various fields, etc. into data structures). But you should not use libraries that do any proxy functions for you.  It is a good opportunity to dive deep into HTTP request and HTTP response.
- The grade will be composed of three parts: functionality/performance, code quality/style (i.e. use of RAII), and danger log

## **Important Reference Resources**

This assignment will require reading RFC specs (i.e. related to HTTP) -- this is the "coding to a spec" part of the assignment.  Here are a few pointers to get started:

- The document that lays out everything about HTTP is https://tools.ietf.org/html/rfc7231. It is ... long. But you don't need most of it. Sections 4,5,6,7 are the most useful.
- https://tools.ietf.org/html/rfc7234 describes the rules of expiration time and cacheability requirements.
- https://tools.ietf.org/html/rfc7230 describes the HTTP format (e.g. section 2 to know where an HTTP request starts / ends)
- Socket programming: consult 650 material and/or this is a great reference: http://beej.us/guide/bgnet/html/multi/index.html
- UNIX daemons: consult our lecture slides

# Tips & **Resources**

<aside>
💡

Below are a few more explanations on the function requirements
- We don't recommend reading at the beginning, as they might seem too detailed
- But you might want to refer to them later when you have questions

</aside>

## vector<char>

A vector<char> is handy for receiving data in your web proxy. It has a .data() which gives you access to the underlying array. You can just pass &v.data()[index] as the pointer for receive() to start writing into, and write straight in the vector (of course tell it how much space there is). If you need more space... you can just grow the vector. Then call receive again at the end index that hasn't been filled up yet. (And remember that receive tells you how many bytes it read, so you'll know how much has been filled up.

## CONNECT

1. When you get a CONNECT request, you need to send a 200 OK to the client after you setup the connection to the server. Otherwise the client does not know you are ready for it to communicate with the server.
2. steps your proxy should take for CONNECT:
    - 1. open a socket with the server (via the connect socket call)
    - 2. send an http response of "200 ok" back to the browser
    - 3. Use IO multiplexing (i.e. select()) from both ports (client and server), simply forwarding messages from one end to another.
    - The proxy does not send a CONNECT http message on to the origin server.

## Payload of HTTP response

Here is a tip I think is worth passing along on reading the RFCs.  Parsing and interpreting HTTP requests and responses appropriately is one of the major tasks in this project, which means you need to read RFC a lot to understand different formats of request and response messages and all those delimiters that tell you when a section should end. For example, the payload of an HTTP response may be returned via different formats.

You may see fields like these in your HTTP header:

`Transfer-Encoding: chunked`

In contrast to the common header field you see:

`Content-Length: <length>`

You may want to handle these two formats differently, and you may find it confusing in the RFCs on understanding the format of message:

```
chunked-body   = *chunk
                      last-chunk
                      trailer-part
                      CRLF

     chunk          = chunk-size [ chunk-ext ] CRLF
                      chunk-data CRLF
     chunk-size     = 1*HEXDIG
     last-chunk     = 1*("0") [ chunk-ext ] CRLF

     chunk-data     = 1*OCTET ; a sequence of chunk-size octets
```

To understand the meaning of symbols, go take a look at [RFC5234](https://tools.ietf.org/html/rfc5234)

## Detached Container vs. Running Proxy as Daemon

Related to the submission of homework 2, you may be having issues running a web proxy as a daemon in the container while setting up your docker configuration. This is a little bit of a tricky point, so it might be helpful to explain a bit.

You might see the container exiting directly after you type docker-compose up, especially if you call the daemon() function to daemonize your proxy. Let's try to understand why this will happen:

Recall that the first step for a process to become daemon is to call fork().

Your proxy process calls fork() to create a child process. Let's temporarily call them proxy_parent and proxy_child.

The proxy_parent will exit once the fork() call returns. The fork() and exit() action is mainly for:

- It unblocks any grandparent process that may be waiting for the parent process to terminate. (For example, if the daemon was invoked from the shell then this returns control to the user.)
- It ensures that the child process is not a process group leader (required for the call to `setsid` performed below).

All seems good so far... But wait a second, a container has its own process namespace, filesystem namespace and user namespace. When you make your proxy process the entry point of the container (i.e. the first process that gets executed when the container starts running), that proxy process will be assigned with pid 1, so the proxy process plays the same role as init process outside of the container. When your proxy process (init process) forks and exits, that means the init process of the container exits, which will destory the whole namespace of this container, therefore making the whole container terminate.

So what should we do if we want to avoid the container exiting while still letting the docker become a daemon inside the container world? We could either:

(1) make another program as entry point, having the program call your proxy server but making the original program never exits. A dummy way that I think of is by making a bash script as your docker entry point and have that script call your proxy server and then have a while 1 loop in that script.

(2) manipulate your proxy server so that when it is launched, it first checked its own pid. If it's one, then it knows it is the init process. It should call fork() as well. However, the parent process should never exit.

Making your proxy server as a daemon in the container doesn't really do any useful thing since you don't have the concept of session in your container (a virtualized world). What you really want to do is to have the whole container run as a daemon (a background process that detaches from the current session so that even if you close the terminal the container is still running, not writing to stdout or stderr). To do that you could simply run **`*docker-compose up -d*`** to detach the container from the current session. To reattach, run  ***docker attach <container-id>***

However, trying to config the proxy server so that it becomes a "daemon" in the container is a good practice for you to understand and refresh how to become a daemon for a process.

If you aren't able to successfully get your "proxy-daemon-in-container" working exactly, you don't need to worry too much about that since if we can run the proxy successfully without the docker, we could still grade your submission.

# Test & debug

## **Proxy Switching**

It is suggested to get a proxy-switching add-on for your browser of choice. One option is [Proxy SwitchSharp](https://chrome.google.com/webstore/detail/proxy-switchysharp/dpplabbmogkhghncfbfdeeokoefdjegm) on Chrome for this. It gives two-click proxy switching in the toolbar.

## Browser for testing Recommendation

For testing your web proxy with browser, Firefox very likely may be the most friendly (and intuitive to change the proxy settings to point to your VM). Below is the instruction to set your web browser, taking Mac OS system for instance.

- In Preferences -- General, drop down to the bottom and click 'Network Settings'. It would be something like those:
- Select 'Manual proxy configuration' and the hostname for you "Http Proxy" could be your vm address.
- Q&As
    - Q: The proxy server is refusing connections?
        
        A: Your settings are good and don't worry about the error message so far.
        The error message shows because you have no proxy programming running in your vm. The netcat program receives the HTTP request from your browser which stored in req.txt, but netcat itself could not send the request to [google.com](http://google.com/) server. So after a while, the netcat prpgram is killed and the TCP connection between your vm and browser ends. Since the Firefox browser (client) did not receive any HTTP Response and the connection is ended, the error message shows up.
        
    

!https://s3-us-west-2.amazonaws.com/secure.notion-static.com/343d1b05-6d40-497a-805f-7b4aed6c40a7/Untitled.png

## netcat

In the end, your caching proxy will be expected to work with requests from a browser.  As a separate tip, you may want to use a browser different from your default one and change its proxy settings to point to your caching proxy.  And, for testing your web proxy with a browser, Firefox very likely may be the most friendly (and intuitive to change the proxy settings to point to your VM).

But while you're getting things going, testing through a browser doesn't give much helpful debug info when things go wrong.

For this, netcat will likely be very helpful.

For example, you can run this command in your terminal:

`netcat -l 8080 > req.txt`

then set the browser's proxy info to that host's port 8080, and make a request.  Now the request is in req.txt. e.g.,

```
GET http://www.man7.org/linux/man-pages/man2/recv.2.html HTTP/1.1
Host: www.man7.org
User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10.13; rv:58.0) Gecko/20100101 Firefox/58.0
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8
Accept-Language: en-US,en;q=0.5
Accept-Encoding: gzip, deflate
Connection: keep-alive
Upgrade-Insecure-Requests: 1
```

Now we want to send this request to the caching proxy, so start up the proxy on your vm and do:

```
cat req.txt | netcat <proxyIP> <proxyPort> > resp.txt
[replace proxyIP and proxyPort appropriately for your caching proxy]
```

Then you can look in resp.txt and see what came back -- even if its not valid html, or not complete, the response is there.

## Mocky.io

[https://www.mocky.io](https://www.mocky.io/)

- Select the desired response status code
- Select content type and enter a body (can just be a simple 'hello world' html page since the content doesn't matter so much)
- Input any custom headers you want! e.g. ETag, Expires, Last-Modified, Cache-Control fields
- Click "Generate Response"
- This will give you a link that you can "GET" and will get a response with the body & headers you set up!
- You can access the generated link through a browser, netcat, etc.

## Test for chunked transfer encoding:

http://www.httpwatch.com/httpgallery/chunked/chunkedimage.aspx

## Test for max-age (max-age is 0)

http://www.artsci.utoronto.ca/futurestudents

## For testing "post"

http://httpbin.org/forms/post

# Submission Details

Here is the submission checklist for HW #2 (similar to what they were for HW #1):

1. Submit your team info to this form ([link](https://forms.gle/YYDA6ueozERnRW4J6)).

2. Fill out the following HW2 Requirements Spreadsheet (posted on Canvas) and make sure it is it in your gitlab repo for HW2.  Follow the same guidelines for filling out this spreadsheet as you did for HW1 (the requirements here are hopefully even more straightforward from the assignment writeup than they were in HW1).

3. Use gitlab to upload your code except the project name should be changed to erss-hwk2-<netid1>-<netid2>.  Push your code with a commit message of "finalize" when you are ready to submit for grading.  Ensure that TAs and instructors are added as reporters to your repo —  **lz238**(Longhao), **hl490**(Evan),  **sx83**(Siyu), **kl461**(Kaixin), **bmr23** (Brian).

4. Make sure your Danger Log is also in your gitlab repo.  For your Danger Log and any readme you have, please make sure those are in Text file format (.txt) as opposed to .pdf or .doc.  Since we grade on a Linux VM, this will make it easier to read your documents.

5. Set up docker for your submission

6. Submit your gitlab link for HW2 to Sakai when you are ready to submit.  Only one person from the group needs to do this.

Please follow the above instructions carefully! 

The structure could be like:

docker-deploy

|—docker-compose.yml

|—src

|—Dockerfile

|—**Other file or directory you have**
