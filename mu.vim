" Vim syntax file
" Language:    mu
" Maintainer:  Kartik Agaram <mu@akkartik.com>
" URL:         http://github.com/akkartik/mu
" License:     public domain
"
" Copy this into your ftplugin directory, and add the following to your vimrc
" or to .vim/ftdetect/mu.vim:
"   autocmd BufReadPost,BufNewFile *.mu set filetype=mu

let s:save_cpo = &cpo
set cpo&vim

" todo: why does this periodically lose syntax, like on file reload?
"   $ vim x.mu
"   :e
"? if exists("b:syntax")
"?   finish
"? endif
"? let b:syntax = "mu"

setlocal iskeyword=@,48-57,?,!,_,$,-
setlocal formatoptions-=t  " Mu programs have long lines
setlocal formatoptions+=c  " but comments should still wrap

syntax match muComment /#.*$/ | highlight link muComment Comment
syntax match muSalientComment /##.*$/ | highlight link muSalientComment SalientComment
syntax match muComment /;.*$/ | highlight link muComment Comment
syntax match muSalientComment /;;.*$/ | highlight link muSalientComment SalientComment
set comments+=n:#
syntax match CommentedCode "#? .*"
let b:cmt_head = "#? "

" Mu strings are inside [ ... ] and can span multiple lines
" don't match '[' at end of line, that's usually code
syntax match muLiteral %^[^ a-zA-Z0-9(){}\[\]#$_*@&,=-][^ ,]*\|[ ,]\@<=[^ a-zA-Z0-9(){}\[\]#$_*@&,=-][^ ,]*%
syntax region muString start=+\[[^\]]+ end=+\]+
syntax match muString "\[\]"
highlight link muString String
" Mu syntax for representing the screen in scenarios
syntax region muScreen start=+ \.+ end=+\.$\|$+
highlight link muScreen muString

" Mu literals
syntax match muLiteral %[^ ]\+:literal/[^ ,]*\|[^ ]\+:literal\>%
syntax match muLiteral %\<[0-9-]\?[0-9]\+/[^ ,]*%
syntax match muLiteral % [0-9-]\?[0-9]\+[, ]\@=\| [0-9-]\?[0-9]\+$%
syntax match muLiteral "^\s\+[^ 0-9a-zA-Z{}$#\[\]][^ ]*\s*$"
" labels
syntax match muLiteral %[^ ]\+:label/[^ ,]*\|[^ ]\+:label\>%
" other literal types
syntax match muLiteral %[^ ]\+:type/[^ ,]*\|[^ ]\+:type\>%
syntax match muLiteral %[^ ]\+:offset/[^ ,]*\|[^ ]\+:offset\>%
syntax match muLiteral %[^ ]\+:variant/[^ ,]*\|[^ ]\+:variant\>%
highlight link muLiteral Constant
syntax keyword muKeyword default-space local-scope next-ingredient next-input ingredient input rewind-ingredients rewind-inputs load-ingredients load-inputs | highlight link muKeyword Constant

syntax match muDelimiter "[{}]" | highlight link muDelimiter Delimiter
syntax match muAssign "<-"
syntax match muAssign "\<raw\>"
highlight link muAssign SpecialChar
syntax match muGlobal %[^ ]\+:global/\?[^ ,]*% | highlight link muGlobal SpecialChar
syntax keyword muControl reply reply-if reply-unless return return-if return-unless output output-if output-unless jump jump-if jump-unless loop loop-if loop-unless break break-if break-unless current-continuation continue-from create-delimited-continuation reply-delimited-continuation | highlight muControl ctermfg=3
" common keywords
syntax match muRecipe "^recipe\>\|^recipe!\>\|^def\>\|^def!\>\|^before\>\|^after\>\| -> " | highlight muRecipe ctermfg=208
syntax match muScenario "^scenario\>" | highlight muScenario ctermfg=34
syntax match muPendingScenario "^pending-scenario\>" | highlight link muPendingScenario SpecialChar
syntax match muData "^type\>\|^container\>\|^exclusive-container\>" | highlight muData ctermfg=226

let &cpo = s:save_cpo
