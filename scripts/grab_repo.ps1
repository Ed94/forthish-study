$path_root      = split-path -Path $PSScriptRoot -Parent
$path_scripts   = join-path $path_root 'scripts'

$misc = join-path $PSScriptRoot 'helpers/misc.ps1'
. $misc

$url_forthish  = 'https://github.com/guitarvydas/forthish.git'
$path_forthish = join-path $path_root 'forthish'

clone-gitrepo $path_forthish $url_forthish


