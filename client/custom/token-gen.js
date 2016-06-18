var fs = require('fs');
var jwt = require('jsonwebtoken');
var process = require('process');
var program = require('commander');

program
    .version('0.0.1')
    .option('-E, --expires_in [value]', 'Expiration expressed in seconds or a string'
        + ' describing a time span. E.g., 60, "2 days", "10h", "7d"', '1h')
    .option('-I, --key_id [value]', 'Id of the encryption key.',
        'f525b853cbd035cc6b2910bb87752311d32091b8')
    .option('-K, --private_key_file [value]', 'Path to the private key file.',
        './private.key')
    .option('-P, --payload_file [value]', 'Path to the JSON payload to be signed.',
        './payload.json')
    .parse(process.argv);

function sign(payload, privateKey) {
  console.log(jwt.sign(JSON.parse(payload), privateKey, {
    algorithm: 'RS256',
    expiresIn: program.expires_in,
    headers: {
      typ: "JWT",
      kid: program.key_id
    }
  }));
}
try {
  var payload = fs.readFileSync(program.payload_file, "utf8");
  var privateKey = fs.readFileSync(program.private_key_file, "utf8");
  sign(payload, privateKey);
} catch(e) {
  console.log(e);
  process.exit(1);
}

