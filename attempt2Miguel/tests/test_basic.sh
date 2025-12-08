#the following are some tests of basic server fucntionality

# Basic functionality test 
curl https://localhost:8080/index.html 
 
# Test HEAD method 
curl -I https://localhost:8080/index.html 
# Test 404 error 
curl https://localhost:8080/nonexistent.html 
 
# Test different file types 
curl https://localhost:8080/index.css           # CSS 
curl https://localhost:8080/index.js            # JavaScript 
curl https://localhost:8080/MainBefore.png      # Image