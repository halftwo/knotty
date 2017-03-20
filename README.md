# knotty
A RPC framework named XIC and its dependence.

The XIC RPC is implemented in C++, PHP (as an extension), and Python (partially implemented).

The data communidated between the XIC servers and clients are serialized using VBS.

VBS is a data serialization method similar JSON in logic. But VBS is binary based instead of text based, which makes it fast to encode and decode compared to JSON.

A XIC server can optionally authenticate the client using SRP6a, which makes the RPC server not be abused by malware. However, the data communicated between servers and clients are not encrypted.
