<?
$str=file_get_contents("log");
//bnetd/server.cpp:918: error: invalid conversion from 'void*' to 't_addr*'
preg_match_all("/(.+):(\d+): error: invalid conversion from '(?:const )?void\*' to '(.+)'/",$str,$mat,PREG_SET_ORDER);
	$deli="(elem_get_data|addr_get_data|x?malloc|x?realloc|packet_get_data_const|packet_get_raw_data|member->memberacc)";
foreach($mat as $k => $v)
{
	$f=file($v[1]);
	
//echo	$f[$v[2]-1]=str_replace("elem_get_data","({$v[3]})elem_get_data",$f[$v[2]-1]);
echo	$f[$v[2]-1]=preg_replace("/{$deli}/","({$v[3]})\\1",$f[$v[2]-1]);

file_put_contents($v[1],implode("",$f));

	print_r($v);
}
//print_r($mat);
?>