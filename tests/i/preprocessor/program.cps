PP_DEFINE MainFlag // File-local and case-sensitive.
PP_DEFINE MainFlag
INCLUDE "peer.cps"
PP_IFDEF MainFlag
OUTPUT 1
PP_ENDIF
PP_IFNDEF mainflag
OUTPUT 2
PP_ENDIF
PP_IFDEF PeerFlag
OUTPUT MissingPeerFlag
PP_ENDIF
PP_IFDEF Absent
"unterminated literal
PP_DEFINE Hidden
PP_UNDEF MainFlag
PP_IFNDEF Other
#INVALID
PP_DEFINE
PP_ENDIF
PP_ENDIF
PP_IFNDEF Hidden
OUTPUT 3
PP_ENDIF
PP_IFDEF MainFlag
OUTPUT 4
PP_ENDIF
PP_UNDEF MainFlag
PP_UNDEF MainFlag
PP_IFDEF MainFlag
OUTPUT MissingMainFlag
PP_ENDIF
PP_IFNDEF MainFlag
    PP_IFNDEF Hidden // Nested enabled conditions.
OUTPUT 5
    PP_ENDIF
PP_ENDIF
DECLARE Flag : INTEGER
PP_DEFINE Flag
Flag <- 6
OUTPUT Flag
CALL Peer()
// PP_IFDEF CommentedOut
OUTPUT "PP_DEFINE Quoted"
