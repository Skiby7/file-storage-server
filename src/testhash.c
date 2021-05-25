#include "common_includes.h"
#include "server.h"
#include "client_queue.h"
#include "file.h"
#include "fssApi.h"
#include <limits.h>

void func(ready_clients *head){
	while (head != NULL){
		printf("%d -> ", head->com);
		head = head->next;
	
	}
	puts("NULL");
	
}

void func1(ready_clients *tail){
	while (tail != NULL){
		printf("%d -> ", tail->com);
		tail = tail->prev;
	}
	puts("NULL");
	
}

static inline unsigned int hash_pjw(void* key){
    char *datum = (char *)key;
    unsigned int hash_value, i;

    if(!datum) return 0;

    for (hash_value = 0; *datum; ++datum) {
        hash_value = (hash_value << ONE_EIGHTH) + *datum;
        if ((i = hash_value & HIGH_BITS) != 0)
            hash_value = (hash_value ^ (i >> THREE_QUARTERS)) & ~HIGH_BITS;
    }
    return (hash_value);
}

static inline unsigned int fnv_hash_function( void *key, int len ) {
    unsigned char *p = (unsigned char*)key;
    unsigned int h = 2166136261u;
    int i;
    for ( i = 0; i < len; i++ )
        h = ( h * 16777619 ) ^ p[i];
    return h;
}

unsigned int hash_val(void* key, unsigned int i, unsigned int max_len, unsigned int key_len){
	return ((hash_pjw(key) + i*fnv_hash_function(key, sizeof(key_len)))%max_len); //i*fnv_hash_function(key, sizeof(key_len))
}


int main(int argc, char* argv[]){
	
	int i = 0;
	int index;
	srand(time(NULL));
	char *path = "/home/leonardo/Documents/SO/Project/file-storage-server/src/hash.c";
	char *path1[] = {"/home/leonardo/file-storage-server/src/hash.c", "/home/leonardo/Documents/SO/Project/file-storage-server/src/fssApi.h", "SO/Project/file-storage-server/build/libutils.a", "script.sh", "/home/leonardo/Documents/SO/Assignments_2/esercizio1.c", "CovidDataAnalysis/README.md", "/tmp/socket", "/var/lib/slafh/cacate", "/home/leonardo/sciaocio", "/home/leonardo", "/home/Documents/Algoritmica", "/mk/aiwwl/luovs/qx", "/uaimwutrhc/wj/pdgdqjlvfu/ysx", "/vlhdbp/eykkldj/bevrsuagz/qeg", "/s/krl/gz/j", "/hr/aip/zkrbxmrguv/jkpi", "/qfbjqgzvm/lx/i/e", "/zmqlfbdw/ejy/upvhqq/fibvdy", "/mfuvhu/rjah/qhwwsgxur/mdr", "/aptomajfij/bat/gngkgcvmwy/ti", "/jnosuk/bjzsot/zla/tdgha", "/wkx/yen/iboappyryu/tmd", "/mur/ytp/hv/rez", "/xcefyiig/vjlm/sgyceogj/zj", "/i/nxklw/cadig/pfazu", "/oxjmgxxqxl/wnz/twikwfpsf/ir", "/unsaxsxvi/ownus/ux/zarq", "/voit/dj/umlag/orauht", "/wxkrohfj/zcyhuvl/zyrpcvcro/r", "/xpnjdcahvu/ddb/msqjn/ibecs", "/zjgpema/gvtndwu/m/bqcp", "/nqv/iq/ixb/cvc", "/lditytxoiz/np/fzpjqwxtl/rwmuxo", "/oobqqtpoa/brzib/rrjnepi/izkuexp", "/aydzil/zv/t/owdr", "/ekbwmxt/ykqrzoh/orfs/l", "/rvdz/vwdksfb/bq/uzxgko", "/ylqgdtf/yr/k/aeoptpa", "/aaxcrcjn/xatji/uwrym/o", "/dohpunungm/z/obqthjxe/yuqj", "/ddna/wozmijv/q/n", "/vynwukks/wics/kbeiwbrg/ndnqm", "/xyxecy/igjpsq/ephxtu/xwxz", "/tlqyyjd/qktsaj/bhstfitv/rilhay", "/zsssg/raw/xynrdlfujw/hbpt", "/s/gi/vlzufvsm/ddidbb", "/juoydvrsul/bowau/ynq/sy", "/jjmvpgiqwj/y/ulgthy/ln", "/buyxabp/jscbnq/y/pprcbe", "/v/milyg/p/esnofum", "/qtk/sns/mdjrv/lrwxnd", "/a/fga/oeqlywc/nlma", "/phrfxelqm/xszjf/h/shl", "/ppg/jo/s/tf", "/q/udox/zk/m", "/yfd/cydxbw/lzwwbu/s", "/jjycp/ibms/eeuthmne/w", "/vkbi/nxbrpw/teiyf/hvq", "/a/wobl/it/umnjgng", "/gdxlqcwb/hnpqck/vyzt/b", "/qra/ynh/bhosttkd/z", "/h/lwezj/foglu/adparj", "/vvwt/wrcbrk/ubh/agx", "/oopdbgck/zwcr/dbpbe/nuxc", "/jqakezmhg/ngxe/jedhkkz/s", "/ipdlmtqlf/pojjq/glrwsq/tjj", "/ptfkpzzo/nht/aw/hucqmxf", "/kus/xcrxovq/rgjneuc/hodqtol", "/wfcfl/pf/evqjhbnjkx/ocrbmz", "/yurtizbetp/xk/mihaqogq/ur", "/mxicd/ynpvmyb/glmiycqiq/hhjsbxl", "/rab/liwbqhl/nqbznndm/ujujjwf", "/id/ldrnu/izqdkhq/h", "/ftloby/wbafri/fafoe/hkkwbqo", "/hpyg/borfa/fao/hetejuv", "/bgite/smzylby/rdqfwpejb/ckroeb", "/jhbmryk/e/vlc/gt", "/tpjxexwmtu/meerw/jxoocjay/k", "/au/uee/s/dnzlh", "/nfqcgub/ozmai/hvaq/cklcggv", "/iltlouwlcf/kyq/eipipjvh/rdm", "/wemw/rbavjb/cj/n", "/jmqnmx/ifxbjsl/c/pl", "/zenojxrf/kguj/ddusd/ksxwgp", "/jcrcdj/zqsrzv/krukfll/bnlwnlz", "/ig/mhoa/kfapiyhe/y", "/ukvhybstp/uacny/rybmyt/v", "/uljqrpr/gkrwua/akqlqbj/wxxp", "/hfmbna/pjedlw/o/aojqpmd", "/fj/cnevwla/ybsmhw/evgdc", "/kounlahn/amfwcu/gohjv/rsordpp", "/hymfrulfnq/c/qoekum/xkrcl", "/xzvnb/wg/ijiadmrp/m", "/aqlwl/qikdzhw/e/ufm", "/bfp/iq/n/wjhm", "/frg/durcipc/nouhb/o", "/zlzb/ddoch/hccgmocrnu/zmyr", "/wnj/rd/moercwclk/qdydmz", "/ofb/wpui/oktls/at", "/qejtwbqa/zdhi/nueett/n", "/dzxrzs/qr/xu/elcptb", "/g/rrvbar/sjskwlnqb/axbf", "/notxqm/ffzhm/qkdrq/zttxeo", "/fwggdffuj/ffaqjf/hmsskuvvfa/rmhx", "/lsehiqzx/bj/h/rbkxi", "/xnqvel/hdcgiu/ttehvaqh/acdddvd", "/vbp/dhn/kkgeuskha/wikc", "/zv/d/rcfgeshehx/pltpesq", "/iiwenbg/jz/bdoqvlbvp/fhfdgxo", "/zn/qnnz/ttbwb/jg", "/kow/q/uriauox/p", "/omhpdmkt/dmiq/usbon/o", "/ckcntovh/ygjfb/atdfum/fqlv", "/fphfj/xhenq/vpahzjpg/pesn", "/vb/so/ej/jmnwwsk", "/amemnitnr/nsh/ysyodgc/u", "/cvvin/trnf/nurvsn/fychmz", "/bhgrec/hweo/qtpw/u", "/voyqwfhmz/dsafdri/xqhjs/i", "/m/afwrav/eshgq/fhfgel", "/tntqdyp/wjfqbte/wvnikk/a", "/duxqqjnqr/rzbna/setg/zw", "/fphl/yvdtqmm/yvsioq/pytrgc", "/c/gwagsi/fsfqejq/hrxbgd", "/blup/pcz/dhnw/axe", "/xtmfa/n/nzadml/pgji", "/xesohsgp/upq/rtyuz/rrmjunt", "/vvtsfrwfi/vevg/tbdfbtznsw/o", "/xpfyr/op/vay/j", "/cpyelgxppu/fo/yg/b", "/kvbd/eqxflwe/qg/zigron", "/bievawosl/cf/ngaccq/mvpnff", "/wj/b/ekzfnaaj/r", "/jes/jjl/xsgh/rtcip", "/wkjt/c/zflmtpthk/huvq", "/yevvl/vrlsikw/g/gaye", "/ap/worhcn/lhvbl/fn", "/ssfdwcrlu/qhpgn/efvg/j", "/bwuifu/tffspl/xwe/g", "/jxwvdm/opuvapu/kuxnacgauy/suajdg", "/hnkvsdh/tkk/nwywxtifrk/csuz", "/ekf/guom/odpyn/ih", "/ht/cxvrwtl/vkrw/iufm", "/eyse/x/xfnjykefm/stxzqv", "/ubo/w/tdpnppmf/ocq", "/uceblcpb/r/wbnfyz/y", "/du/h/ue/pktvkvw", "/deipjmsuo/hqjg/yu/hbcrrpn", "/xefhnf/xevzl/mfohepwqvx/rcsfh", "/k/y/jroiaa/jd", "/kowhciycit/ppcuajh/aofuaf/henar", "/hzkid/bpqrum/s/pjh", "/t/o/j/xt", "/edcig/ur/csjvdtyo/bzrvrs", "/coflhtnt/syyqejh/tohd/gpghyb", "/wwaii/t/owmhfu/br", "/jlwgtnvlz/lwbsywt/fxaskzof/mtiaz", "/slmkt/vj/eby/cy", "/dvsdctvq/yoetxu/zqlzcrgc/ebclb", "/dmxn/mlibe/oqmju/mlp", "/xlldffz/vlm/liqh/tjtmbdm", "/pczcdfklb/iwf/wbbg/dk", "/awxkzyu/rw/mtidsroeik/cexz", "/loaqj/ij/uwihu/ewhc", "/cpk/oorzwb/oys/oxa", "/dvmsnvuw/c/lednj/lwatns"};
	int storage_size = 336;
	int filenum = 166;
	int storage[storage_size];
	int done = 1;
	int takes = 0;
	char *file = NULL;
	int count = 0;
	int j = 0;
	memset(storage, 0, storage_size*sizeof(int));
	while(1){
		if (j == filenum)
			break;
		file = path1[j];
		index =  hash_val(file, i++, storage_size, strlen(file));
		takes++;
		if(storage[index] == 0){
			i = 0;
			j++;
			storage[index]++;
			
		}
		else{
			i++;
			continue;
		}
			
		// for (size_t i = 0; i < storage_size; i++)
		// {
		// 	if(i == index)
		// 		printf(ANSI_COLOR_RED"%d "ANSI_COLOR_RESET, storage[i]);
		// 	else
		// 		printf("%d ", storage[i]);
		// 	if(storage[i] == 0)
		// 		done = 0;
		// }
		// puts("");
		
		// if(done)
		// 	break;
		
	}
	int cluster = 0;
	j = 0;
	for(int i = 0; i < filenum;i++){
		j = i;
		while(j < filenum && storage[j] == 1) j++;
		
		if((j - i) + 1 > 2){
			cluster++;
		}i
		 = j;
		
		
	}
	double access, size;
	access = takes;
	size = filenum;
	printf("It takes -> %d tries\nacesses\\file -> %.2f\nCluster -> %d\n", takes, access/size, cluster);
	return 0;

}


