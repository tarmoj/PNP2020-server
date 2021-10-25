// WEBSOCKET FUNCTIONS -----------------------------------
	
	//var serverURL = "ws://johannes.homeip.net:8008/ws";
	
	function doConnect(url, onEvent)
	{
		websocket = new WebSocket(url);
		websocket.onopen = function(evt) { onOpen(evt) };
		websocket.onclose = function(evt) { onClose(evt) };
		websocket.onmessage = function(evt) { 
			if (typeof(onEvent)!=="undefined") {
				onEvent(evt);
			}
		
			if (typeof(onMessage)!=="undefined") {
				onMessage(evt);
			}
		};
		websocket.onerror = function(evt) { onError(evt) };
		
	}

	function onOpen(evt)
	{
		writeToScreen("connected\n");
		document.getElementById("connectButton").disabled = true;
		document.getElementById("connectButton").innerHTML = "Connected";
		if (typeof(myOnOpen) !== "undefined") { // do something when socket is opened
			myOnOpen();
		}
	}

	function onClose(evt)
	{
		writeToScreen("state: disconnected\n");
		document.getElementById("connectButton").disabled = false;
		document.getElementById("connectButton").innerHTML = "Connect";
	}

	

	function onError(evt)
	{
		writeToScreen('error (' + document.getElementById("url").value + ') ' + evt.data + '\n');
		websocket.close();
		document.getElementById("connectButton").disabled = false;
		document.getElementById("connectButton").innerHTML = "Connect";
	}

	function doSend(message)
	{
		// how to check if websocket is open?
		if (websocket.readyState == 1) {
			websocket.send(message);
			writeToScreen("sent: " + message + '\n');
		} else 
			writeToScreen("WebSocket is not open!");
	}
	
	function doSendArray(arr) // the data must be an array of the type that the server knows to expect
	{
		if (websocket.readyState == 1) {
			//var arr = new Float64Array(data); // to convert for any case
			websocket.send(arr);
			writeToScreen("sent " + arr.length*arr.BYTES_PER_ELEMENT + 'bytes \n');
		} else 
			writeToScreen("WebSocket is not open!");
	}
	
	function doSendNumber(number) // UNTESTED!
	{
		if (websocket.readyState == 1) {
			var arr = new Float64Array([number]);
			websocket.send(new Float64Array([number]));
			writeToScreen("sent " + arr.length*arr.BYTES_PER_ELEMENT + 'bytes \n');
		} else 
			writeToScreen("WebSocket is not open!");
	}

	
	function writeToScreen(message)
	{
	//alert(message);
	var outputtext = document.getElementById("outputtext");
	outputtext.value += message;
	outputtext.scrollTop = outputtext.scrollHeight;
	}
