// the source of the web page we serve

char CONFIG_PAGE[] = R"(<html>
<head></head>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<style>
body {
font-family: apercu-pro, -apple-system, system-ui, BlinkMacSystemFont, 'Helvetica Neue', sans-serif;
padding: 1em;
line-height: 2em;
font-weight: 500;
}
td {
font-weight: 500;
min-height: 24px;
}
td:first-child { 
text-align: right;
min-width: 100px;
padding-right: 10px;
}
h1 {
font-size: 1.5em;
font-weight: 700;
}
h2 {
font-size: 1.2em;
font-weight: 500;
margin-left: 5px;
}
input {
border: 1px solid rgb(196, 196, 196);
color: rgb(76, 76, 76);
width: 240px;
border-radius: 3px;
height: 40px;
margin: 3px 0px;
padding: 0px 14px;
}
input:focus {
border:1px solid black;
outline: none !important;
box-shadow: 0 0 10px #719ECE;
}
#config {
width:400px; 
margin:0 auto;
}
.ok-button {
background-color: #0078e7;
color: #fff;
border-radius: 5px;
}
.red-button {
background-color: #e72e00;
color: #fff;
border-radius: 5px;
}
#led {
background-color: #%s;
border: 0px;
height: 10px;
width: 10px;
border-radius: 5px;
}
img {
float:right;
}
</style>
<body>
<div id='config'>
<h1> NAT Router Config <img src="data:image/jpeg;base64,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" 
alt="T4ME2 logo" /> </h1>
<script>
if (window.location.search.substr(1) != '')
{
document.getElementById('config').display = 'none';
if (window.location.search.includes('reset'))
{
document.body.innerHTML ='<h1>NAT Router Config</h1>The device will be reset.<br/>The page will refresh soon automatically...';
setTimeout("location.href = '/'",10000);
}
else 
{
document.body.innerHTML ='<h1>NAT Router Config</h1>The new settings have been sent to the device.<br/>The page will refresh soon automatically...';
setTimeout("location.href = '/'",3000);
}
}
</script>
<h2>AP Settings (the new network)</h2>
<form action='' method='GET'>
<table>
<tr>
<td>SSID</td>
<td><input type='text' name='ap_ssid' value='%s' placeholder='SSID of the new network'/></td>
</tr>
<tr>
<td>Password</td>
<td><input type='text' name='ap_password' value='%s' placeholder='Password of the new network'/></td>
</tr>
<tr>
<td></td>
<td><input type='submit' value='Set' class='ok-button'/></td>
</tr>
</table>
<small>
<i>Password </i>less than 8 chars = open<br />
</small>
</form>
<h2>STA Settings (uplink WiFi network) <button id="led"></button></h2>
<form action='' method='GET'>
<table>
<tr>
<td>SSID</td>
<td><input type='text' name='sta_ssid' value='%s' placeholder='SSID of existing network'/></td>
</tr>
<tr>
<td>Password</td>
<td><input type='text' name='sta_password' value='%s' placeholder='Password of existing network'/></td>
</tr>
<!--
<tr>
<td colspan='2'>WPA2 Enterprise settings. Leave blank for regular</td>
</tr>
<tr>
<td>Username</td>
<td><input type='text' name='ent_username' value='%s' placeholder='WPA2 Enterprise username'/></td>
</tr>
<tr>
<td>Identity</td>
<td><input type='text' name='ent_identity' value='%s' placeholder='WPA2 Enterprise identity'/></td>
</tr>
-->
<tr>
<td></td>
<td><input type='submit' value='Connect' class='ok-button'/></td>
</tr>
</table>
</form>
<h2>STA Static IP Settings</h2>
<form action='' method='GET'>
<table>
<tr>
<td>Static IP</td>
<td><input type='text' name='static_ip' value='%s'/></td>
</tr>
<tr>
<td>Subnet Mask</td>
<td><input type='text' name='subnet_mask' value='%s'/></td>
</tr>
<tr>
<td>Gateway</td>
<td><input type='text' name='gateway' value='%s'/></td>
</tr>
<tr>
<td></td>
<td><input type='submit' value='Connect' class='ok-button'/></td>
</tr>
</table>
<small>
<i>Leave it blank if you want to get an IP using DHCP</i>
</small>
</form>
<h2>Device Management</h2>
<form action='' method='GET'>
<table>
<tr>
<td>Device</td>
<td><input type='submit' name='reset' value='Reboot' class='red-button'/></td>
</tr>
</table>
</form>
<h2>Firmware upload</h2>
<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>
<table>
<tr>
<td>Firmware</td><td>
<input type='file' name='update'>
<input type='submit' value='Update' class='red-button'>
</td>
</tr>
</table>
</form>
<table>
<tr>
<td></td><td>
<div id='prg'>progress: </div>
</td>
</tr>
</table>
<script>
var form = document.querySelector('#upload_form');
var per = 0;
form.addEventListener('submit', (evt) => {
evt.preventDefault();
var xhr = new XMLHttpRequest();
var data = new FormData(form);
function updateProgress (evt) {
if (evt.lengthComputable) {
per = evt.loaded / evt.total;
document.querySelector('#prg').innerHTML = ('progress: ' + Math.round(per * 100) + '%%');
}
else document.querySelector('#prg').innerHTML = ('progress: ---');
};
function loadProgress (evt) {
if (evt.lengthComputable) {
per = evt.loaded / evt.total;
document.querySelector('#prg').innerHTML = ('progress: ' + Math.round(per * 100) + '%% '+ xhr.responseText);
if(xhr.responseText.includes("reboot")) setTimeout("location.href = '/'",5000);
}
else document.querySelector('#prg').innerHTML = ('progress: --- ' + xhr.responseText);
console.log(xhr.responseText);
};
xhr.upload.addEventListener('progress', updateProgress);
xhr.addEventListener('loadend', loadProgress);
xhr.open('POST', '/update');
xhr.send(data);
});
</script>
</div> 
</body>
</html>
)";
