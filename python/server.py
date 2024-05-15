from flask import Flask, request
import socket

app = Flask(__name__)

def get_local_ip():
    try:
        # Get the hostname
        hostname = socket.gethostname()
        # Get the local IP address associated with the hostname
        local_ip = socket.gethostbyname(hostname)
        return local_ip
    except socket.error as e:
        return f"Unable to get local IP: {e}"

@app.route('/post_data', methods=['POST'])
def post_data():
    key = request.form.get('batata')
    if key:
        print(f"Received key: batata, value: {key}")
        return "Data received", 200
    else:
        return "No data received", 400

if __name__ == '__main__':
    local_ip = get_local_ip()
    print(f"Local IP Address: {local_ip}")
    app.run(host='0.0.0.0', port=5000, debug=True)  # Bind to all interfaces
  
