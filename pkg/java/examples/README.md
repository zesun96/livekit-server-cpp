# Java server SDK examples

Build the bridge and Java library as described in [the binding README](../README.md).
From the `livekit-server-cpp` directory, compile the example:

```powershell
javac --release 17 -encoding UTF-8 -cp out/build/livekit-server-java-0.1.0.jar -d out/build/java-examples (rg --files pkg/java/examples/java | Where-Object { $_ -like '*.java' })
```

To explicitly run a real-server request, set `LIVEKIT_URL` and either `LIVEKIT_TOKEN` or
`LIVEKIT_API_KEY`/`LIVEKIT_API_SECRET` in your shell, then run:

```powershell
java "-Dlivekit.server.java.library=$PWD/out/build/java-binding-static/Release/livekitserver_java.dll" -cp "out/build/livekit-server-java-0.1.0.jar;out/build/java-examples" io.livekit.server.examples.ListRooms
```

Use `:` instead of `;` as the classpath separator on Linux/macOS and select the corresponding
native library path. This example lists rooms and prints their JSON; it does not create rooms.
The offline smoke test in `src/test/java` requires no server or credentials.
