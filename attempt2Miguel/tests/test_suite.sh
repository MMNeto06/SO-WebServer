#the following are some tests of basic server fucntionality

# Basic functionality test 
curl http://localhost:8080/index.html 
 
# Test HEAD method 
curl -I http://localhost:8080/index.html 
# Test 404 error 
curl http://localhost:8080/nonexistent.html 
 
# Test different file types 
curl http://localhost:8080/index.css        # CSS 
curl http://localhost:8080/index.js        # JavaScript 
curl http://localhost:8080/MainBefore.png        # Image