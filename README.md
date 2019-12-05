# image marker
Half-assed example service that puts text on an image. And client, too.

Number of maximum concurrent jobs is configurable.
Client retries if server is busy.

Based on Boost.Asio and OpenCV

## example usage:

./server --port 1300 --jobs 4

./client localhost 1300 "example text" input.jpg output.png

