# knotty
A RPC framework named XIC and its dependencies.

The XIC RPC is implemented in C++, PHP (as an extension), and Python (partially implemented).

The data communicated between the XIC servers and clients are serialized using VBS.

VBS is a data serialization method similar to JSON in structure. But VBS is binary based instead of text based, which makes it fast to encode and decode compared with JSON.

A XIC server can optionally authenticate the client using SRP6a, which makes the RPC server not be abused by malware. 
The communications between server and client can be encrypted and authenticated with AES-EAX if configured.
