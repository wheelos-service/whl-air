// signaling_server/src/server.js
const WebSocket = require('ws');
const http = require('http'); // Use http for non-SSL, https for SSL
const https = require('https');
const fs = require('fs');
const url = require('url'); // To parse URL for token
const config = require('../config'); // Load configuration
const auth = require('./auth'); // Authentication module
const sessionManager = require('./session_manager'); // Session management module

// --- Setup HTTP/HTTPS server for WebSocket ---
let server;
const serverOptions = {}; // Options for https server

if (config.server.ssl.enabled) {
    try {
        serverOptions.key = fs.readFileSync(config.server.ssl.key);
        serverOptions.cert = fs.readFileSync(config.server.ssl.cert);
        server = https.createServer(serverOptions);
        console.log(`Server: HTTPS server created. SSL enabled.`);
    } catch (err) {
        console.error(`Server: Failed to read SSL certificates: ${err.message}`);
        console.error("Server: SSL will not be enabled. Check certs/server.key and certs/server.crt.");
        server = http.createServer(); // Fallback to http if certs fail
    }
} else {
    server = http.createServer();
    console.log(`Server: HTTP server created. SSL disabled.`);
}

// Create WebSocket server instance
const wss = new WebSocket.Server({ server });

// --- WebSocket Server Event Handlers ---

wss.on('listening', () => {
    console.log(`Server: WebSocket server started on port ${config.server.port}`);
});

wss.on('connection', async (ws, request) => {
    console.log('Server: New WebSocket connection received.');

    // --- Authentication ---
    // Expect JWT token in a query parameter, e.g., ws://.../?token=YOUR_JWT
    const parameters = url.parse(request.url, true).query;
    const token = parameters.token;

    const clientId = await auth.verifyToken(token);

    if (!clientId) {
        console.warn('Server: WebSocket connection rejected: Invalid or missing token.');
        ws.send(JSON.stringify({ type: 'error', message: 'Authentication failed' }));
        ws.terminate(); // Close connection immediately
        return;
    }

    // Assign a simple ID to the WebSocket object for easier logging/tracking (optional)
    ws.id = clientId; // Use authenticated client ID as connection ID

    console.log(`Server: WebSocket connection authenticated for client: ${clientId}`);

    // Add client to session manager
    sessionManager.addClient(ws, clientId);

    // --- Message Handling ---
    ws.on('message', (message) => {
        console.log(`Server: Received message from ${clientId}: ${message}`);
        try {
            const parsedMessage = JSON.parse(message);
            // Route the message via the session manager
            sessionManager.routeMessage(ws, parsedMessage);
        } catch (e) {
            console.error(`Server: Failed to parse message from ${clientId}: ${e.message}`);
            // Optional: Send error back to client
             ws.send(JSON.stringify({ type: 'error', message: `Failed to parse message: ${e.message}` }));
        }
    });

    // --- Connection Closing Handling ---
    ws.on('close', (code, reason) => {
        console.log(`Server: WebSocket connection closed for ${clientId}. Code: ${code}, Reason: ${reason}`);
        // Remove client from session manager
        sessionManager.removeClient(ws);
    });

    // --- Error Handling ---
    ws.on('error', (error) => {
        console.error(`Server: WebSocket error for ${clientId}: ${error.message}`);
        // The 'close' event will usually follow an error, so removal happens there.
    });

    // Optional: Send a confirmation message to the client
    ws.send(JSON.stringify({ type: 'connected', clientId: clientId }));
});

// Handle server errors (e.g., port already in use)
server.on('error', (err) => {
    console.error(`Server: HTTP/HTTPS server error: ${err.message}`);
    process.exit(1); // Exit process if the server cannot start
});

// --- Start the HTTP/HTTPS server ---
server.listen(config.server.port);

// --- Handle process termination ---
process.on('SIGINT', () => {
    console.log('Server: SIGINT received. Shutting down gracefully.');
    wss.close(() => {
        console.log('Server: WebSocket server closed.');
        server.close(() => {
            console.log('Server: HTTP/HTTPS server closed.');
            process.exit(0);
        });
    });
});

process.on('SIGTERM', () => {
     console.log('Server: SIGTERM received. Shutting down gracefully.');
     wss.close(() => {
         console.log('Server: WebSocket server closed.');
         server.close(() => {
             console.log('Server: HTTP/HTTPS server closed.');
             process.exit(0);
         });
     });
 });
