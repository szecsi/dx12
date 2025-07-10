uint maskOffset : register(b0);
					// u0: mortons in
					// u1: indices in
					// u2: sorted indices
					// u3: sorted mortons
					// u4: #starters per page | #leading non-starters per page
					// u5: hlist
					// u6: cbegin | clength
					// u7: sorted cbegin
					// u8: sorted hlist
					// u9: h: #starters per page | #leading non-starters pp
					// ua: embedflag: hstart | hlength OR cbegin | clength


//RWByteAddressBuffer inputMortons : register(u0);
//RWByteAddressBuffer sorted : register(u1); // morton or hash
//RWByteAddressBuffer starterCounts : register(u2);
//RWByteAddressBuffer hlist : register(u3);
//RWByteAddressBuffer cbegin : register(u5);
//RWByteAddressBuffer hbegin : register(u6);

#define rowSize 32
#define nRowsPerPage 32

