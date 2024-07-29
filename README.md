This is our final year project

MultiThreaded Proxy Server Which caches the responses from the server there by delivering faster access of responses if the user requests the data again, The Proxy concept uses LRU cache mechanism to store data.

HOW TO EXECUTE THE CODE

First cd to ProxyServer and type command "make" in it

For running type ./proxy <YOUR_SPECIFIC_PORT> 

Finally your proxy server is up and running.

For getting the results to be displayed on a website,

You will be needing the WebLogger which is basically built on flask, install all the necessary libraries which are

pip install flask flask_socketio //this will install the necessary libraries

cd to the WebLogger and type:
Python flask_server.py


