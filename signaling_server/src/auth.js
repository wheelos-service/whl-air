// signaling_server/src/auth.js
const jwt = require('jsonwebtoken');
const config = require('../config'); // Load configuration

/**
 * Verifies a JWT token and extracts the client ID.
 * @param {string} token - The JWT token string.
 * @returns {Promise<string|null>} - Resolves with the client ID if valid, or null if invalid.
 */
async function verifyToken(token) {
    if (!token) {
        return null; // No token provided
    }
    try {
        const decoded = jwt.verify(token, config.auth.jwtSecret);
        // Assumes the JWT payload includes a 'clientId' field
        if (decoded && decoded.clientId) {
            console.log(`Auth: Token verified for client ${decoded.clientId}`);
            return decoded.clientId;
        }
        return null; // Token valid but no clientId in payload
    } catch (err) {
        console.error(`Auth: Token verification failed: ${err.message}`);
        return null; // Token is invalid (expired, wrong signature, etc.)
    }
}

module.exports = {
    verifyToken
};
