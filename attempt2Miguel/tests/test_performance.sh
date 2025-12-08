time curl -s https://localhost:8080/index.html > /dev/null 
 
# Subsequent requests (cache hit) 
for i in {1..10}; do 
    time curl -s https://localhost:8080/index.html > /dev/null 
done 
 
# Monitor server resource usage 
top -p $(pgrep -f "./server") 
 
# Monitor worker processes 
watch -n 1 'pgrep -P $(pgrep -f "./server") | xargs ps -o 
pid,ppid,nlwp,cmd' 