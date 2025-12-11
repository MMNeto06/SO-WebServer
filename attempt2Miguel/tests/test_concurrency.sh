# Parallel requests with curl 
for i in {1..100}; do 
    curl -s https://localhost:8080/index.html & 
done 

for i in {1..50}; do 
    curl -s https://localhost:8080/page$((i % 10)).html & 
done 
wait