import socket, ssl
import threading, asyncio
from typing import Callable

class DMClient():
    """
    DMClient
    --------
    This is the simple client of the Death March server Project. Which allows a simple way to connect, send
    and receive information between a dmserver and the program that uses this client. 
    Everything under TCP with optional TLS encryption.

    It is thought to be used as an asyncronous and non-blocking client which allows compatibility with more complex frameworks
    that use their internal syncronous loops.
    """
    def __init__(self, shost="127.0.0.1", sport=2020, crb_len=4096, cuse_ssl=False):
        """ Initialization of the class.
        
        Args:
            - shost(str): IP(v4 or v6) of the dmserver host.
            - sport(int): Port number of the dmserver host.
            - crbuf_len(int): Number of bytes to read at once from the dmserver com.
            - cuse_ssl(bool): Boolean to use or not to use secure socket layer (must be supported by the dmserver host). """
        # Connection related attributes:
        self._shost = shost
        self._sport = sport
        self._cuse_ssl = cuse_ssl
        self._cctx = None
        if self._cuse_ssl:
            self._sslConf()

        # Communication related attributes:
        self._cwriter = None
        self._creader = None
        self._crb_len = crb_len

        # User related attributes:
        self._on_receive = None
        self._on_disconnect = None

        # Loop control and non-blocking behaveour attributes:
        self._running = False
        self._loop = None
        self._th = threading.Thread(target=self._cloop, daemon=True)

    def setOnDisconnect(self, callback: Callable[[None], None]):
        """ Function to set a callback to be executed when the client disconnects from the dmserver.

        Args:
            - callback(Callable[[None], None]): Function reference that takes no arguments and returns nothing.
                                                Executed in the read loop thread when a disconnection happens.
        """
        # In case callback si not valid ignore call:
        if not callable(callback):
            return False

        # Callback set:
        self._on_disconnect = callback
        return True
        
    def setOnReceive(self, callback: Callable[[str], None]) -> bool:
        """ Function to set a callback to be executed when new daata is received.

        Args:
            - callback(Callable[[str], None]): Function reference that takes a single string argument and
                                               returns nothing. Executed in the read loop thread.

        Return:
            - bool: False if the callback wasn't set, true otherwise. """
        # In case callback is not valid ignore call:
        if not callable(callback):
            return False

        # Callback set:
        self._on_receive = callback
        return True

    def isRunning(self) -> bool:
        """ Function to return the current state of the client.

        Return:
            - bool: False if disconnected, true if connected. """
        return self._running

    def connect(self) -> bool:
        """ Function to create a connection between the client and the server,
        running as a separated thread and asyncio logic. 
        
        Return:
            - bool: False if is already connected, or true if the process started correctly. """
        # In case is already running (connected) ignore the call:
        if self._running:
            return False

        # Set the running state and starts the new thread (where the connection is handled):
        self._running = True
        self._th.start()
        return True

    def disconnect(self) -> bool:
        """ Function to disconnect the client from the dmserver.
        
        Return:
            - bool: False if already disconnected, or true if disconnection succeeded."""
        # In case is not running (disconnected) ignore call:
        if not self._running:
            return False

        # Force the end of the reader thread and disconnect gracefuly:
        self._running = False
        return True

    def send(self, data: str) -> bool:
        """ Function to send data to the server side. This is the syncronous side of the call,
        once called, it execute the data transmission in the secondary thread as asyncronous. 
        
        Args:
            - data(str): The data string to transmit as is.
            
        Return:
            - bool: False if is disconnected, or true if the process """
        # In case is not running (disconnected) ignore the call:
        if not self._running:
            return False

        # In case data is not a string or is not valid ignore the call:
        if not isinstance(data, str) or not data:
            return False

        # Start the data transmission asyncronously on the secondary thread, in a safe way:
        asyncio.run_coroutine_threadsafe(self._send(data), self._loop)
        return True

    def _sslConf(self):
        """ Configure the ssl context for TLSv1.3 with no hostname checking or verification(unsafe). """
        self._cctx = ssl.create_default_context(ssl.Purpose.SERVER_AUTH)
        self._cctx.maximum_version = ssl.TLSVersion.TLSv1_3
        self._cctx.minimum_version = ssl.TLSVersion.TLSv1_3
        self._cctx.check_hostname = False
        self._cctx.verify_mode = ssl.CERT_NONE

    async def _send(self, data: str):
        """ Coroutine to send data over the client writer stream. 
        
        Args:
            - data(str): The data string to transmit as is."""
        # In case the client writer stream is not valid, ignore call:
        if not self._cwriter:
            return

        # Write the data to the writer stream and send it.
        self._cwriter.write(data.encode('utf-8'))
        await self._cwriter.drain()

    async def _connect(self):
        """ Coroutine to establish a secure connection to the dmserver. """
        try:
            # Try to connect to the dmserver with the configured connection data:
            self._creader, self._cwriter = await asyncio.open_connection(
                self._shost,
                self._sport,
                ssl=self._cctx if self._cuse_ssl else None,
                server_hostname=self._shost if self._cuse_ssl else None
            )
            
        except Exception as e:
            self._running = False

    async def _disconnect(self):
        """ Coroutine to properly close the connection and clean up. """
        self._running = False

        # Safely close the write streams and sets streams to default values:
        if self._cwriter:
            self._cwriter.close()
            await self._cwriter.wait_closed()
            self._cwriter = None
            self._creader = None

        # Execute the corresponding callback if set:
        if self._on_disconnect:
            try:
                self._on_disconnect()
            except Exception as cb_e:
                pass

    def _cloop(self) -> None:
        """ Function (thread target) that launch the client working coroutine loop until is closed. """
        # Create the asyncronous loop of the client and add it to the event loop:
        self._loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self._loop)

        # Execute the coroute loop that reads data from the server side until it closes:
        try:
            self._loop.run_until_complete(self._read_loop())
        except Exception as e:
            self._running = False
        finally:
            self._loop.close()
        
    async def _read_loop(self) -> None:
        """ Coroutine (main reading loop) that continuously reads data from the server side. If
        no data is received, connection is assumed as closed. """
        try:
            # Previous connections with server side:
            await self._connect()

            # Read loop:
            while self._running:
                data = await self._creader.read(self._crb_len)

                # If no data is received, a forced disconnection is assumed:
                if not data:
                    break

                # Execute the user defined callback:
                if self._on_receive:
                    try:
                        self._on_receive(data.decode('utf-8'))
                    except Exception as cb_e:
                        pass
                
        except Exception as e:
            pass
        finally:
            # Clean disconnection after loop ended:
            await self._disconnect()


if __name__ == "__main__":
    def rcv_user_callback(msg: str):
        print(f"[<<] Recibido: {msg}")

    def dc_user_callback():
        print("[!] Advertencia: Cliente desconectado.")
        cli.disconnect()

    cli = DMClient(cuse_ssl=True)
    cli.setOnDisconnect(dc_user_callback)
    cli.setOnReceive(rcv_user_callback)
    cli.connect()

    c = ""
    while c != "exit()" and cli.isRunning():
        cli.send(c) 
        c = input(">> ")

