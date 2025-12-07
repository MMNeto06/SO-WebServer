# Basic load test (1000 requests, 10 concurrent) 
ab -n 1000 -c 10 https://localhost:8080/index.html 
 
# High concurrency test (10000 requests, 100 concurrent) 
ab -n 10000 -c 100 https://localhost:8080/index.html 
 
# Sustained load test (5 minutes) 
ab -t 300 -c 50 https://localhost:8080/ 
 
# Test multiple files 
for file in index.html index.css index.js; do 
    ab -n 1000 -c 50 https://localhost:8080/$file 
done