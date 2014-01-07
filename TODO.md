Tumbleweed
==========

Full Range Requests support. Really?
------------------------------------

* Even multiple ranges in one request with non-overlapping  multipart replies? Many webservers do not aggregate the request. There is a lot of potential for off-by-one errors.
* Also support negative and empty range offsets? This is not sane...
* zsyncs first request is out of boundary right from the beginning. Most webservers ignore this and just fix the broken range. zsync does ask for multiple ranges in one request later.
* Mozilla discusses to remove multiple ranges in one request, because they do not get it right internally for themselves.

What are ranges good for anyway? zsync, HTML5 Videos and Bittorrent Webseeds? However, this adds a lot of complexity...
