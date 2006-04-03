/*
 *  xmm_power.c
 *  xmmLibm
 *
 *  Created by Ian Ollmann, Ph.D. on 7/15/05.
 *  Copyright © 2005 Apple Computer, Inc. All rights reserved.
 *
 *  Constants from original typing by Earl Killian of MIPS on March 23rd, 1992.         
 *  Converted from pairs of 32-bit hexadecimal words to C99 notation, by Ian Ollmann
 *
 *  Algorithm from Peter Tang:
 *
 *      ACM Transactions on Mathematical Software, Vol 15, 2, June 1989, pp 144-157
 *      ACM Transactions on Mathematical Software, Vol 18, 2, June 1992, pp 211-222
 *      
 */
 
#if defined( __i386__ )
 #include "xmmLibm_prefix.h"

//#include "math.h"

static const double T1 =  0x1.F800000000000p-1; //         0.984375 //  T1 = 1-1/64
static const double T2 =  0x1.0400000000000p+0; //         1.015625 //  T2 = 1+1/64
static const double T52 = 0x1.0000000000000p+52; // 4503599627370496  //  T52 = 2^52
static const double T53 = 0x1.0000000000000p+53; // 9007199254740992  //  T53 = 2^53
static const double Emax =  0x1.62E42FEFA39EFp+9; //709.782712893383973096
static const double Emin = -0x1.74385446D71C3p+9; //-744.44007192138121809
static const double A1 =  0x1.5555555555555p-4; //0.0833333333333333287074
static const double A2 =  0x1.99999999FE736p-7; //0.0125000000007165872062
static const double A3 =  0x1.24919E6600596p-9; //0.00223212297987691443008
static const double B1 =  0x1.555555554F9EDp-4; //0.0833333333330085884727
static const double B2 =  0x1.999A118F10BD9p-7; //0.0125000558601921375051
static const double C1 =  0x1.5555555555555p-4; //0.0833333333333333287074
static const double C2 =  0x1.99999999AF7C8p-7; //0.0125000000001555117146
static const double C3 =  0x1.24923B2F1315Cp-9; //0.00223214123210461849733
static const double C4 =  0x1.CE20EE795DBA1p-12; //0.00044072021372392784647
static const double C5 =  0x1.CE20EE795DBA1p-12; //0.00044072021372392784647
static const double Inv_L =  0x1.71547652B82FEp+5; //46.1662413084468283841
static const double L1 =  0x1.62E42FEF00000p-6; //0.0216608493901730980724
static const double L2 =  0x1.473DE6AF278EDp-39; //2.32519284687887401481e-12
static const double P1 =  0x1.0000000000000p-1; //              0.5
static const double P2 =  0x1.5555555547334p-3; //0.166666666665059914898
static const double P3 =  0x1.555555554A9D2p-5; //0.0416666666663619983391
static const double P4 =  0x1.1111609FBE568p-7; //0.00833337038010269204324
static const double P5 =  0x1.6C172098CB78Bp-10; //0.0013888944287816253325
static const double plusinf =  1e500; //              inf

static const double logtable[129*2] = {
                                         0.0, //                0
                                         0.0, //                0
                                         0x1.FE02A6B200000p-8, //0.00778214044294145423919
                                        -0x1.F30EE07912DF9p-41, //-8.86505291726724739329e-13
                                         0x1.FC0A8B1000000p-7, //0.015504186536418274045
                                        -0x1.FE0E183092C59p-42, //-4.5301989413649348858e-13
                                         0x1.7B91B07D80000p-6, //0.0231670592820591991767
                                        -0x1.2772AB6C0559Cp-41, //-5.24820947929564425056e-13
                                         0x1.F829B0E780000p-6, //0.0307716586667083902285
                                         0x1.980267C7E09E4p-45, //4.52981425779092882775e-14
                                         0x1.39E87BA000000p-5, //0.0383188643027096986771
                                        -0x1.42A056FEA4DFDp-41, //-5.7309948330766312415e-13
                                         0x1.77458F6340000p-5, //0.0458095360318111488596
                                        -0x1.2303B9CB0D5E1p-41, //-5.16945692881222029089e-13
                                         0x1.B42DD71180000p-5, //0.053244514518155483529
                                         0x1.71BEC28D14C7Ep-41, //6.56799336898521766515e-13
                                         0x1.F0A30C0100000p-5, //0.0606246218158048577607
                                         0x1.62A6617CC9717p-41, //6.29984819938331143924e-13
                                         0x1.16536EEA40000p-4, //0.0679506619089806918055
                                        -0x1.0A3E2F3B47D18p-41, //-4.72942410916632905482e-13
                                         0x1.341D7961C0000p-4, //0.0752234212377516087145
                                        -0x1.717B6B33E44F8p-43, //-1.64083015855986619259e-13
                                         0x1.51B073F060000p-4, //0.0824436692109884461388
                                         0x1.83F69278E686Ap-44, //8.61451293608781447223e-14
                                         0x1.6F0D28AE60000p-4, //0.089612158690215437673
                                        -0x1.2968C836CC8C2p-41, //-5.28305053080814367877e-13
                                         0x1.8C345D6320000p-4, //0.0967296264589094789699
                                        -0x1.937C294D2F567p-42, //-3.58366674300941370128e-13
                                         0x1.A926D3A4A0000p-4, //0.103796793680885457434
                                         0x1.AAC6CA17A4554p-41, //7.58107392301637643358e-13
                                         0x1.C5E548F5C0000p-4, //0.110814366340491687879
                                        -0x1.C5E7514F4083Fp-43, //-2.01573684160162150917e-13
                                         0x1.E27076E2A0000p-4, //0.117783035655520507134
                                         0x1.E5CBD3D50FFFCp-41, //8.62947404296943765415e-13
                                         0x1.FEC9131DC0000p-4, //0.12470347850103280507
                                        -0x1.54555D1AE6607p-44, //-7.55692068745133691756e-14
                                         0x1.0D77E7CD10000p-3, //0.131576357789526809938
                                        -0x1.C69A65A23A170p-41, //-8.075373495358435282e-13
                                         0x1.1B72AD52F0000p-3, //0.138402322858382831328
                                         0x1.9E80A41811A39p-41, //7.36304357708705134617e-13
                                         0x1.29552F8200000p-3, //0.145182009844575077295
                                        -0x1.5B967F4471DFCp-44, //-7.71800133682809851086e-14
                                         0x1.371FC201F0000p-3, //0.151916042026641662233
                                        -0x1.C22F10C9A4EA8p-41, //-7.99687160774375808217e-13
                                         0x1.44D2B6CCB0000p-3, //0.158605030175749561749
                                         0x1.F4799F4F6543Ep-41, //8.890223439724662699e-13
                                         0x1.526E5E3A20000p-3, //0.165249572895845631137
                                        -0x1.2F21746FF8A47p-41, //-5.38468261878823239516e-13
                                         0x1.5FF3070A80000p-3, //0.171850256927427835763
                                        -0x1.B0B0DE3077D7Ep-41, //-7.6861342240181686233e-13
                                         0x1.6D60FE71A0000p-3, //0.178407657473144354299
                                        -0x1.6F1B955C4D1DAp-42, //-3.26057179310581566492e-13
                                         0x1.7AB8902110000p-3, //0.184922338494288851507
                                        -0x1.37B720E4A694Bp-42, //-2.76858843104483055456e-13
                                         0x1.87FA065210000p-3, //0.191394853000019793399
                                        -0x1.B77B7EFFB7F41p-42, //-3.90338789379495198219e-13
                                         0x1.9525A9CF40000p-3, //0.197825743329303804785
                                         0x1.5AD1D904C1D4Ep-41, //6.16075577558872326221e-13
                                         0x1.A23BC1FE30000p-3, //0.20421554142922104802
                                        -0x1.2A739B23B93E1p-41, //-5.30156516006025969342e-13
                                         0x1.AF3C94E810000p-3, //0.210564769107804750092
                                        -0x1.00349CC67F9B2p-41, //-4.55112422774782019696e-13
                                         0x1.BC286742E0000p-3, //0.216873938301432644948
                                        -0x1.CCA75818C5DBCp-41, //-8.18285329273778346373e-13
                                         0x1.C8FF7C79B0000p-3, //0.223143551314933574758
                                        -0x1.97794F689F843p-41, //-7.23818992174968106057e-13
                                         0x1.D5C216B500000p-3, //0.229374101065332069993
                                        -0x1.11BA91BBCA682p-41, //-4.86240001538378988236e-13
                                         0x1.E27076E2B0000p-3, //0.235566071312860003673
                                        -0x1.A342C2AF0003Cp-44, //-9.30945949519688945136e-14
                                         0x1.EF0ADCBDC0000p-3, //0.241719936886511277407
                                         0x1.64D948637950Ep-41, //6.33890736899755317832e-13
                                         0x1.FB9186D5E0000p-3, //0.247836163904139539227
                                         0x1.F1546AAA3361Cp-42, //4.41717553713155466566e-13
                                         0x1.0402594B50000p-2, //0.253915209981641964987
                                        -0x1.7DF928EC217A5p-41, //-6.78520849597058843023e-13
                                         0x1.0A324E2738000p-2, //0.259957524436686071567
                                         0x1.0E35F73F7A018p-42, //2.39995404842117353465e-13
                                         0x1.1058BF9AE8000p-2, //0.265963548497893498279
                                        -0x1.A9573B02FAA5Ap-41, //-7.55556940028374187478e-13
                                         0x1.1675CABAB8000p-2, //0.271933715483100968413
                                         0x1.30701CE63EAB9p-41, //5.40790418614551497411e-13
                                         0x1.1C898C1698000p-2, //0.277868451003087102436
                                         0x1.9FAFBC68E7540p-42, //3.69203750820800887027e-13
                                         0x1.22941FBCF8000p-2, //0.283768173130738432519
                                        -0x1.A6976F5EB0963p-44, //-9.38341722366369999987e-14
                                         0x1.2895A13DE8000p-2, //0.289633292582948342897
                                         0x1.A8D7AD24C13F0p-44, //9.43339818951269030846e-14
                                         0x1.2E8E2BAE10000p-2, //0.2954642128934210632
                                         0x1.D309C2CC91A85p-42, //4.1481318704258567983e-13
                                         0x1.347DD9A988000p-2, //0.301261330578199704178
                                        -0x1.5594DD4C58092p-45, //-3.7923164802093146798e-14
                                         0x1.3A64C55698000p-2, //0.307025035295737325214
                                        -0x1.D0B1C68651946p-41, //-8.25463138725003992339e-13
                                         0x1.4043086868000p-2, //0.312755710003330023028
                                         0x1.3F1DE86093EFAp-41, //5.66865358290073900922e-13
                                         0x1.4618BC21C8000p-2, //0.318453731119006988592
                                        -0x1.09EC17A426426p-41, //-4.72372782198636743149e-13
                                         0x1.4BE5F95778000p-2, //0.324119468654316733591
                                        -0x1.D7C92CD9AD824p-44, //-1.04757500587765412913e-13
                                         0x1.51AAD872E0000p-2, //0.329753286372579168528
                                        -0x1.F4BD8DB0A7CC1p-44, //-1.11186713895593226425e-13
                                         0x1.5767717458000p-2, //0.335355541921671829186
                                        -0x1.2C9D5B2A49AF9p-41, //-5.33998929200329680741e-13
                                         0x1.5D1BDBF580000p-2, //0.340926586970454081893
                                         0x1.394A11B1C1EE4p-43, //1.39128412121975659358e-13
                                         0x1.62C82F2BA0000p-2, //0.34646676734701031819
                                        -0x1.C356848506EADp-41, //-8.01737271397201833369e-13
                                         0x1.686C81E9B0000p-2, //0.351976423156884266064
                                         0x1.4AEC442BE1015p-42, //2.9391859187648000773e-13
                                         0x1.6E08EAA2B8000p-2, //0.357455888921322184615
                                         0x1.0F1C609C98C6Cp-41, //4.81589611172320539489e-13
                                         0x1.739D7F6BC0000p-2, //0.362905493690050207078
                                        -0x1.7FCB18ED9D603p-41, //-6.81753940632532726416e-13
                                         0x1.792A55FDD8000p-2, //0.368325561159508652054
                                        -0x1.C2EC1F512DC03p-41, //-8.00999005543249141653e-13
                                         0x1.7EAF83B828000p-2, //0.373716409792905324139
                                         0x1.7E1B259D2F3DAp-41, //6.78756682315870616582e-13
                                         0x1.842D1DA1E8000p-2, //0.379078352934811846353
                                         0x1.62E927628CBC2p-43, //1.57612037739694350287e-13
                                         0x1.89A3386C18000p-2, //0.384411698911208077334
                                        -0x1.ED2A52C73BF78p-41, //-8.76037599077487426799e-13
                                         0x1.8F11E87368000p-2, //0.389716751140440464951
                                        -0x1.D3881E8962A96p-42, //-4.15251580634361213004e-13
                                         0x1.947941C210000p-2, //0.394993808240542421117
                                         0x1.6FABA4CDD147Dp-42, //3.26556988969071456956e-13
                                         0x1.99D9581180000p-2, //0.400243164127459749579
                                        -0x1.F753456D113B8p-42, //-4.47042650104524445082e-13
                                         0x1.9F323ECBF8000p-2, //0.405465108107819105498
                                         0x1.84BF2B68D766Fp-42, //3.45276479520397708744e-13
                                         0x1.A484090E58000p-2, //0.410659924984429380856
                                         0x1.D8515FE535B87p-41, //8.39005077851830734139e-13
                                         0x1.A9CEC9A9A0000p-2, //0.415827895143593195826
                                         0x1.0931A909FEA5Ep-43, //1.17769787513692141889e-13
                                         0x1.AF12932478000p-2, //0.420969294644237379543
                                        -0x1.E53BB31EED7A9p-44, //-1.07743414616095792458e-13
                                         0x1.B44F77BCC8000p-2, //0.426084395310681429692
                                         0x1.EC5197DDB55D3p-43, //2.1863343293215910319e-13
                                         0x1.B985896930000p-2, //0.431173464818130014464
                                         0x1.0FB598FB14F89p-42, //2.41326394913331314894e-13
                                         0x1.BEB4D9DA70000p-2, //0.436236766774527495727
                                         0x1.B7BF7861D37ACp-42, //3.90574622098307022265e-13
                                         0x1.C3DD7A7CD8000p-2, //0.44127456080423144158
                                         0x1.6A6B9D9E0A5BDp-41, //6.43787909737320689684e-13
                                         0x1.C8FF7C79A8000p-2, //0.446287102628048160113
                                         0x1.A21AC25D81EF3p-42, //3.71351419195920213229e-13
                                         0x1.CE1AF0B860000p-2, //0.451274644139630254358
                                        -0x1.8290905A86AA6p-43, //-1.71669213360824320344e-13
                                         0x1.D32FE7E010000p-2, //0.456237433481874177232
                                        -0x1.42A9E21373414p-42, //-2.86582851579143533167e-13
                                         0x1.D83E7258A0000p-2, //0.461175715121498797089
                                         0x1.79F2828ADD176p-41, //6.71369279138460114513e-13
                                         0x1.DD46A04C20000p-2, //0.466089729925442952663
                                        -0x1.DAFA08CECADB1p-41, //-8.43728104087127572675e-13
                                         0x1.E24881A7C8000p-2, //0.470979715219073113985
                                        -0x1.3D9E34270BA6Bp-42, //-2.82101438461812689978e-13
                                         0x1.E744261D68000p-2, //0.475845904869856894948
                                         0x1.E1F8DF68DBCF3p-44, //1.07019317621142549209e-13
                                         0x1.EC399D2468000p-2, //0.480688529345570714213
                                         0x1.9802EB9DCA7E7p-43, //1.81193463664411114729e-13
                                         0x1.F128F5FAF0000p-2, //0.485507815781602403149
                                         0x1.BB2CD720EC44Cp-44, //9.84046527823262695501e-14
                                         0x1.F6123FA700000p-2, //0.490303988044615834951
                                         0x1.45630A2B61E5Bp-41, //5.78003198945402769376e-13
                                         0x1.FAF588F790000p-2, //0.495077266798034543172
                                        -0x1.9C24CA098362Bp-43, //-1.83028573560416684376e-13
                                         0x1.FFD2E08580000p-2, //0.499827869556611403823
                                        -0x1.6CF54D05F9367p-43, //-1.62074001567449500378e-13
                                         0x1.02552A5A5C000p-1, //0.504556010751912253909
                                         0x1.0FEC69C695D7Fp-41, //4.830331494955320223e-13
                                         0x1.04BDF9DA94000p-1, //0.509261901790523552336
                                        -0x1.92D9A033EFF75p-41, //-7.15605531723821174215e-13
                                         0x1.0723E5C1CC000p-1, //0.513945751101346104406
                                         0x1.F404E57963891p-41, //8.8821239518571854472e-13
                                         0x1.0986F4F574000p-1, //0.518607764208354637958
                                        -0x1.5BE8DC04AD601p-42, //-3.09005805132382426227e-13
                                         0x1.0BE72E4254000p-1, //0.523248143765158602037
                                        -0x1.57D49676844CCp-41, //-6.10765519728514951184e-13
                                         0x1.0E44985D1C000p-1, //0.527867089620485785417
                                         0x1.917EDD5CBBD2Dp-42, //3.56599696633478298092e-13
                                         0x1.109F39E2D4000p-1, //0.532464798869114019908
                                         0x1.92DFBC7D93617p-42, //3.57823965912763837621e-13
                                         0x1.12F7195940000p-1, //0.537041465897345915437
                                        -0x1.043ACFEDCE638p-41, //-4.62260870015445768969e-13
                                         0x1.154C3D2F4C000p-1, //0.541597282432121573947
                                         0x1.5E9A98F33A396p-41, //6.22797629172251525649e-13
                                         0x1.179EABBD88000p-1, //0.54613243759740726091
                                         0x1.9A0BFC60E6FA0p-41, //7.28389472720657362987e-13
                                         0x1.19EE6B467C000p-1, //0.550647117952394182794
                                         0x1.2DD98B97BAEF0p-42, //2.68096466152116723636e-13
                                         0x1.1C3B81F714000p-1, //0.555141507540611200966
                                        -0x1.EDA1B58389902p-44, //-1.09608250460592783688e-13
                                         0x1.1E85F5E704000p-1, //0.559615787935399566777
                                         0x1.A07BD8B34BE7Cp-46, //2.3119493838005377632e-14
                                         0x1.20CDCD192C000p-1, //0.564070138285387656651
                                        -0x1.4926CAFC2F08Ap-41, //-5.84690580052992446699e-13
                                         0x1.23130D7BEC000p-1, //0.568504735352689749561
                                        -0x1.7AFA4392F1BA7p-46, //-2.10374825114449422873e-14
                                         0x1.2555BCE990000p-1, //0.572919753562018740922
                                        -0x1.06987F78A4A5Ep-42, //-2.33231829455874083248e-13
                                         0x1.2795E1289C000p-1, //0.577315365035246941261
                                        -0x1.DCA290F81848Dp-42, //-4.23336942881419153586e-13
                                         0x1.29D37FEC2C000p-1, //0.5816917396350618219
                                        -0x1.EEA6F465268B4p-42, //-4.39339379697378426136e-13
                                         0x1.2C0E9ED448000p-1, //0.586049045003164792433
                                         0x1.D1772F5386374p-42, //4.13416470738355643357e-13
                                         0x1.2E47436E40000p-1, //0.590387446602107957006
                                         0x1.34202A10C3491p-44, //6.84176364159146659095e-14
                                         0x1.307D7334F0000p-1, //0.594707107746216934174
                                         0x1.0BE1FB590A1F5p-41, //4.75855340044306376333e-13
                                         0x1.32B1339120000p-1, //0.599008189645246602595
                                         0x1.D71320556B67Bp-41, //8.36796786747576938145e-13
                                         0x1.34E289D9D0000p-1, //0.603290851438941899687
                                        -0x1.E2CE9146D277Ap-41, //-8.57637346466586390378e-13
                                         0x1.37117B5474000p-1, //0.607555250224322662689
                                         0x1.ED71774092113p-43, //2.19132812293400917731e-13
                                         0x1.393E0D3564000p-1, //0.611801541106615331955
                                        -0x1.5E6563BBD9FC9p-41, //-6.22428425364311469584e-13
                                         0x1.3B6844A000000p-1, //0.616029877215623855591
                                        -0x1.EEA838909F3D3p-44, //-1.0983594325438429833e-13
                                         0x1.3D9026A714000p-1, //0.620240409751204424538
                                         0x1.6FAA404263D0Bp-41, //6.53104313776336534177e-13
                                         0x1.3FB5B84D18000p-1, //0.624433288012369303033
                                        -0x1.0BDA4B162AFA3p-41, //-4.75801990217107657695e-13
                                         0x1.41D8FE8468000p-1, //0.628608659422752680257
                                        -0x1.AA33736867A17p-42, //-3.78542512654570397313e-13
                                         0x1.43F9FE2F9C000p-1, //0.632766669570628437214
                                         0x1.CCEF4E4F736C2p-42, //4.09392332186786656392e-13
                                         0x1.4618BC21C4000p-1, //0.636907462236194987781
                                         0x1.EC27D0B7B37B3p-41, //8.74243839148582888557e-13
                                         0x1.48353D1EA8000p-1, //0.641031179420679109171
                                         0x1.1BEE7ABD17660p-42, //2.52181884568428814231e-13
                                         0x1.4A4F85DB04000p-1, //0.645137961373620782979
                                        -0x1.44FDD840B8591p-45, //-3.60813136042255739798e-14
                                         0x1.4C679AFCD0000p-1, //0.64922794662561500445
                                        -0x1.1C64E971322CEp-41, //-5.05185559242808977623e-13
                                         0x1.4E7D811B74000p-1, //0.653301272011958644725
                                         0x1.BB09CB0985646p-41, //7.86994033233553225797e-13
                                         0x1.50913CC018000p-1, //0.657358072709030238912
                                        -0x1.794B434C5A4F5p-41, //-6.70208769619490643267e-13
                                         0x1.52A2D265BC000p-1, //0.661398482245203922503
                                         0x1.6ABB9DF22BC57p-43, //1.61085757539324585156e-13
                                         0x1.54B2467998000p-1, //0.665422632544505177066
                                         0x1.497A915428B44p-41, //5.85271884362515112697e-13
                                         0x1.56BF9D5B40000p-1, //0.669430653942981734872
                                        -0x1.8CD7DC73BD194p-42, //-3.52467572979047943315e-13
                                         0x1.58CADB5CD8000p-1, //0.673422675212350441143
                                        -0x1.9DB3DB43689B4p-43, //-1.83720844956290584735e-13
                                         0x1.5AD404C358000p-1, //0.677398823590920073912
                                         0x1.F2CFB29AAA5F0p-41, //8.86066898134949155668e-13
                                         0x1.5CDB1DC6C0000p-1, //0.681359224807238206267
                                         0x1.7648CF6E3C5D7p-41, //6.64862680714687006264e-13
                                         0x1.5EE02A9240000p-1, //0.685304003098281100392
                                         0x1.67570D6095FD2p-41, //6.38316151706465171657e-13
                                         0x1.60E32F4478000p-1, //0.689233281238557538018
                                         0x1.1B194F912B417p-42, //2.51442307283760746611e-13
                                         0x1.62E42FEFA4000p-1, //0.693147180560117703862
                                        -0x1.8432A1B0E2634p-43 //-1.72394445256148347926e-13
                                    };
                                    
static const double exptable[32*2] = {
                                         0x1.0000000000000p+0, //                1
                                         0.0,                   //                0
                                         0x1.059B0D3158540p+0, //1.02189714865410508082
                                         0x1.A1D73E2A475B4p-47, //1.15974117063913618369e-14
                                         0x1.0B5586CF98900p+0, //1.04427378242741042413
                                         0x1.EC5317256E308p-49, //3.41618797093084913461e-15
                                         0x1.11301D0125B40p+0, //1.06714040067681992241
                                         0x1.0A4EBBF1AED93p-48, //3.69575974405711634226e-15
                                         0x1.172B83C7D5140p+0, //1.09050773266524458904
                                         0x1.D6E6FBE462876p-47, //1.30701638697787231928e-14
                                         0x1.1D4873168B980p+0, //1.11438674259588310633
                                         0x1.53C02DC0144C8p-47, //9.42997619141976990955e-15
                                         0x1.2387A6E756200p+0, //1.13878863475667913008
                                         0x1.C3360FD6D8E0Bp-47, //1.25236260025620074908e-14
                                         0x1.29E9DF51FDEC0p+0, //1.16372485877757014805
                                         0x1.09612E8AFAD12p-47, //7.36576401089527357793e-15
                                         0x1.306FE0A31B700p+0, //1.18920711500271636396
                                         0x1.52DE8D5A46306p-48, //4.70275685574031410345e-15
                                         0x1.371A7373AA9C0p+0, //1.21524735998046651275
                                         0x1.54E28AA05E8A9p-49, //2.36536434724852953227e-15
                                         0x1.3DEA64C123400p+0, //1.2418578120734764525
                                         0x1.11ADA0911F09Fp-47, //7.59609684336943426188e-15
                                         0x1.44E0860618900p+0, //1.26905095719172322788
                                         0x1.68189B7A04EF8p-47, //9.99467515375775096979e-15
                                         0x1.4BFDAD5362A00p+0, //1.29683955465100098081
                                         0x1.38EA1CBD7F621p-47, //8.68512209487110863385e-15
                                         0x1.5342B569D4F80p+0, //1.32523664315974087913
                                         0x1.DF0A83C49D86Ap-52, //4.15501897749673983948e-16
                                         0x1.5AB07DD485400p+0, //1.35425554693688354746
                                         0x1.4AC64980A8C8Fp-47, //9.18083828572431297142e-15
                                         0x1.6247EB03A5580p+0, //1.38390988196383091235
                                         0x1.2C7C3E81BF4B7p-50, //1.04251790803720876383e-15
                                         0x1.6A09E667F3BC0p+0, //1.41421356237309225889
                                         0x1.921165F626CDDp-49, //2.78990693089087774733e-15
                                         0x1.71F75E8EC5F40p+0, //1.44518080697703510396
                                         0x1.9EE91B8797785p-47, //1.15160818747516875124e-14
                                         0x1.7A11473EB0180p+0, //1.47682614593949779191
                                         0x1.B5F54408FDB37p-50, //1.51947228890629129108e-15
                                         0x1.82589994CCE00p+0, //1.50916442759341862256
                                         0x1.28ACF88AFAB35p-48, //4.11720196080016564552e-15
                                         0x1.8ACE5422AA0C0p+0, //1.54221082540793474891
                                         0x1.B5BA7C55A192Dp-48, //6.074702681072821835e-15
                                         0x1.93737B0CDC5C0p+0, //1.57598084510787828094
                                         0x1.27A280E1F92A0p-47, //8.20551346575487959595e-15
                                         0x1.9C49182A3F080p+0, //1.61049033194925073076
                                         0x1.01C7C46B071F3p-48, //3.57742087137029902059e-15
                                         0x1.A5503B23E2540p+0, //1.64575547815395850648
                                         0x1.C8B424491CAF8p-48, //6.33803674368915982631e-15
                                         0x1.AE89F995AD380p+0, //1.68179283050741901206
                                         0x1.6AF439A68BB99p-47, //1.00739973218322238167e-14
                                         0x1.B7F76F2FB5E40p+0, //1.71861929812247637983
                                         0x1.BAA9EC206AD4Fp-50, //1.5357984302925880313e-15
                                         0x1.C199BDD855280p+0, //1.75625216037329323626
                                         0x1.C2220CB12A092p-48, //6.24685034485536557515e-15
                                         0x1.CB720DCEF9040p+0, //1.79470907500309806437
                                         0x1.48A81E5E8F4A5p-47, //9.12205626035419583226e-15
                                         0x1.D5818DCFBA480p+0, //1.83400808640934087634
                                         0x1.C976816BAD9B8p-50, //1.58714330671767538549e-15
                                         0x1.DFC97337B9B40p+0, //1.87416763411029307917
                                         0x1.EB968CAC39ED3p-48, //6.82215511854592947014e-15
                                         0x1.EA4AFA2A490C0p+0, //1.91520656139714162691
                                         0x1.9858F73A18F5Ep-48, //5.66696026748885461802e-15
                                         0x1.F50765B6E4540p+0, //1.95714412417540017941
                                         0x1.9D3E12DD8A18Bp-54 //8.9607677910366677676e-17
 };

static const double one = 1.0;
static const double mOne = -1.0;
static const double two = 2.0;
static const xSInt64 bias[2] = { {2045, 0},  {1023, 0} };
static const xUInt64 POWER_NAN = { 0x7FF8000000000025ULL, 0 };     //should be the same as nan("37")


static inline xDouble ipow( xDouble x, uint32_t u ) ALWAYS_INLINE;
static inline xDouble _pow( xDouble X, xDouble Y ) ALWAYS_INLINE;
 
static inline xDouble xscalb( xDouble x, int M )
{
    static const double scale[2] = { 0x1.0p1022, 0x1.0p-1022 };
    static const int    step[2] = { 1022, -1022 };
    int index = M >> 31;
    int count = abs(M);
    xSInt64 xm = _mm_cvtsi32_si128( M + 1023 );
    xSInt64 xstep = _mm_cvtsi32_si128( step[-index] ); 
    xDouble xscale = _mm_load_sd( scale - index );
        
    if( count > 1022 )
    {
        x = _mm_mul_sd( x, xscale );
        xm = _mm_sub_epi64( xm, xstep );
        count -= 1022;

        if( count > 1022 )
        {
            x = _mm_mul_sd( x, xscale );
            xm = _mm_sub_epi64( xm, xstep );
            count -= 1022;

            if( count > 1022 )
                return _mm_mul_sd( x, xscale );
        }
    }

    xm = _mm_slli_epi64( xm, 52 );
    return _mm_mul_sd( x, (xDouble) xm );
} 
 
 //The original source from vMathLib used recursion (yuck!) 
 //That was removed in favor of this more efficient loop
static inline xDouble ipow( xDouble x, uint32_t u )
{
    xDouble		result;
    uint32_t    mask = 1;

	if( 0 == u )
		return _mm_load_sd( &one );
	
	//skip trailing zeros
    while( u && 0 == (u & mask) )
    {
        //square x
        x = _mm_mul_sd( x,x );    

        //move up to the next mask
        mask += mask;
    }

	//this prevents an underflow from occurring for pow( tiny, 1 )
	result = x;

	//clear the bit in u (so we don't loop forever)
	u &= ~mask;

    while( u )
    {
        //square x
        x = _mm_mul_sd( x,x );    

        //move up to the next mask
        mask += mask;

        //if the current mask bit is set in u, multiply the result by x
        if( u & mask )
		{
            result = _mm_mul_sd( result, x );

			//clear the bit in u (so we don't loop forever)
			u &= ~mask;
		}
    }
 
    return result;
}

 
static inline xDouble _pow( xDouble X, xDouble Y )
{
    //get old fp env, and set the default one
    int oldMXCSR = _mm_getcsr();
	int newMXCSR = (oldMXCSR | DEFAULT_MXCSR) & DEFAULT_MASK;		//set standard masks, enable denormals, set round to nearest
	if( newMXCSR != oldMXCSR )
		_mm_setcsr( newMXCSR );

    static const double tenM20 = 1e-20;
    static const double ten20 = 1e20;
    static const double d16 = 16.0;
    static const double d128 = 128.0;
    static const double r128 = 1.0/128.0;
    static const double r8 = 1.0/8.0;
    static const double smallestNormal = 0x1.0p-1022;
    static const xUInt64 iOne = { 1, 0 };


	//if y == 0 or x == 1, then the answer is always 1, even if the other argument is NaN 
	if( _mm_istrue_sd( _mm_or_pd( _mm_cmpeq_sdm( X, &one ), _mm_cmpeq_sdm( Y, (double*) &minusZeroD ) )) )
	{        
		X = _mm_load_sd( &one );
		if( newMXCSR != oldMXCSR )
			_mm_setcsr( oldMXCSR );
		return X;
	}

    //get NaN inputs out of the way
    if( _mm_istrue_sd( _mm_cmpunord_sd( X, Y ) ) )
    {
		if( newMXCSR != oldMXCSR )
			_mm_setcsr( oldMXCSR );
        return _mm_add_sd( X, Y ); 
    }

    xDouble fabsY = _mm_andnot_pd( minusZeroD, Y );
    xDouble fabsX = _mm_andnot_pd( minusZeroD, X );
	xDouble fabsyIsInf = _mm_cmpeq_sdm( fabsY, &plusinf );
    xDouble signPatch = _mm_setzero_pd();

	//detect if Y is integer, even or odd
	xDouble floorbias = _mm_load_sd( &T52 );
		floorbias = _mm_and_pd( floorbias, _mm_cmplt_sd( fabsY, floorbias ) ); 
    xDouble yShifted = _mm_add_sd( fabsY, floorbias);
    xDouble roundFabsY = _mm_sub_sd( yShifted, floorbias );
    xDouble yIsEven = (xDouble) _mm_sub_epi64( _mm_and_si128( (xUInt64) yShifted, iOne ), iOne );   
    xDouble yIsInt = _mm_cmpeq_sd( roundFabsY, fabsY);
    int intY = _mm_cvtsd_si32( Y );
	_mm_setcsr( newMXCSR );                                                     //Avoid setting the inexact flag
	yIsEven = _mm_or_pd( yIsEven, _mm_cmpge_sdm( fabsY, &T53 ) );				//|y| >= 2**53 is always even
	yIsInt = _mm_andnot_pd( fabsyIsInf,  yIsInt );


    //Patch up the case of negative X with large integer Y. We have to do it here, 
    //because recursion is not inlinable. 
	xDouble fabsxLTInf = _mm_cmplt_sdm( fabsX, &plusinf );
    if( _mm_istrue_sd( _mm_and_pd( _mm_cmplt_sdm( X, (double*) &minusZeroD ), fabsxLTInf )))     //X < 0
    {
        if( _mm_istrue_sd( yIsInt ) )
        {
            X = fabsX;                 //X = fabs(X)
            signPatch = _mm_andnot_pd( yIsEven, minusZeroD );   //flip the sign before exiting if y is an odd int

			//if x == -1, then the answer is always 1 or -1, unless NaN (removed above) 
			if( _mm_istrue_sd( _mm_cmpeq_sdm( X, &one )) )
			{        
				X = _mm_load_sd( &one );
				goto exit;
			}
        }
        else
        {
			if( _mm_istrue_sd( _mm_cmplt_sdm( fabsY, &plusinf ) ) )
			{
				X = (xDouble) POWER_NAN;
				oldMXCSR |= INVALID_FLAG;
				goto exit;
			}
        }
    }

    xDouble xGTzero = _mm_cmpgt_sdm( X, (double*) &minusZeroD ); 
    xDouble yLTzero = _mm_cmplt_sdm( Y, (double*) &minusZeroD ); 
    xDouble xEQzero = _mm_cmpeq_sdm( X, (double*) &minusZeroD );
        
    //More accurate, fast case for Y is a integer
    if( _mm_istrue_sd( _mm_andnot_pd( xEQzero, yIsInt)) && intY != 0x80000000 )
    {
		if(  intY < 0 )
		{
			_mm_setcsr( DEFAULT_MXCSR ); 
			xDouble recip = _mm_div_sd( _mm_load_sd( &one ), X );
			
			//if the reciprocal is exact, use ipow
			if( 0 == (_mm_getcsr() & INEXACT_FLAG ) )
			{
				X = ipow( recip, -intY );        
				oldMXCSR |= _mm_getcsr() & ALL_FLAGS;
				_mm_setcsr( oldMXCSR );
				X = _mm_xor_pd( X, signPatch );
				return X;
			}
			_mm_setcsr( newMXCSR );      
		}
		else
		{
			X = ipow( X, intY );        
			oldMXCSR |= _mm_getcsr() & ALL_FLAGS;
			_mm_setcsr( oldMXCSR );
			X = _mm_xor_pd( X, signPatch );
			return X;
		}
    }

    xDouble fabsyEQhalf = _mm_cmpeq_sdm( fabsY, &P1 );

    //sqrt, reciprocal sqrt
    if( _mm_istrue_sd( _mm_and_pd( fabsyEQhalf, xGTzero ) ) )
    {
        _mm_setcsr( oldMXCSR );
        X = _MM_SQRT_SD( X );
        if( _mm_istrue_sd( yLTzero ) )
            X = _mm_div_sd( _mm_load_sd( &one ), X );
        return X;
    }


    xDouble xLTinf =  _mm_cmplt_sdm( X, &plusinf );
    xDouble fabsyGT1em20 = _mm_cmpgt_sdm( fabsY, &tenM20 );
    xDouble fabsyLT1e20 = _mm_cmplt_sdm( fabsY, &ten20 );
    
    if( _mm_istrue_sd( _mm_and_pd(  _mm_and_pd( xGTzero, xLTinf ),                      //x > 0 && x < inf
                                    _mm_and_pd( fabsyGT1em20, fabsyLT1e20 ) ) ) )       //|y| > 1e-20 && |y| < 1e20
    {
        xDouble z1, z2, w1, w2;
        xDouble t1LTx = _mm_cmpgt_sdm( X, &T1 );
        xDouble xLTt2 = _mm_cmplt_sdm( X, &T2 );
        if( _mm_istrue_sd( _mm_and_pd( t1LTx, xLTt2 ) ) )
        {
			/* |X - 1| < 2^-6 */
			/* procedure Lsmall */
            xDouble xEQone = _mm_cmpeq_sdm( X, &one );
            if( _mm_istrue_sd( xEQone ) )
            {
                X = _mm_load_sd( &one );
                goto exit;
            }
            xDouble f = _mm_sub_sdm( X, &one );
            xDouble f1 = _mm_cvtss_sd( f, _mm_cvtsd_ss( (xFloat)f, f ));
            xDouble f2 = _mm_sub_sd( f, f1 );
            xDouble g = _mm_add_sdm( f, &two );
            g = _mm_div_sd( _mm_load_sd( &one ), g );
            xDouble u = _mm_mul_sd( f, g );
            u = _mm_add_sd( u, u );
            xDouble v = _mm_mul_sd( u, u );
            xDouble q = _mm_add_sdm( _mm_mul_sdm( v, &C5 ), &C4 );
            q = _mm_add_sdm( _mm_mul_sd( q, v ), &C3 );
            q = _mm_add_sdm( _mm_mul_sd( q, v ), &C2 );
            q = _mm_add_sdm( _mm_mul_sd( q, v ), &C1 );
            v = _mm_mul_sd( v, u );
            q = _mm_mul_sd( q, v );
        
            xDouble u1 = _mm_cvtss_sd( u, _mm_cvtsd_ss( (xFloat)u, u ));
            xDouble u2 = _mm_sub_sd( f, u1 );
            u2 = _mm_add_sd( u2, u2 );
            u2 = _mm_sub_sd( u2, _mm_mul_sd( u1, f1 ) );
            u2 = _mm_sub_sd( u2, _mm_mul_sd( u1, f2 ) );
            u2 = _mm_mul_sd( u2, g );
            z2 = _mm_add_sd( q, u2 );
            z1 = _mm_add_sd( u1, z2 );
            z1 = _mm_cvtss_sd( z1, _mm_cvtsd_ss( (xFloat)z1, z1 ));
            z2 = _mm_add_sd( z2, _mm_sub_sd( u1, z1 ));
        }
        else
        {
            //			x = mantissa(X, &m);  //  m = logb(X);  x = scalb(X, -m);
            //convert denormals to normals
            xDouble xOne = _mm_load_sd( &one );
            xDouble isDenormal = _mm_cmplt_sdm( fabsX, &smallestNormal );
            xDouble denormBias = _mm_and_pd( xOne, isDenormal );
            int isdenorm = _mm_cvtsi128_si32(  (xSInt64) isDenormal );
            X = _mm_or_pd( X, denormBias );
            X = _mm_sub_sd( X, denormBias );
    
            //calculate mantissa and exponent
            xDouble inf = _mm_load_sd( &plusinf );
            xDouble exponent = _mm_and_pd( X, inf);
            xDouble x = _mm_andnot_pd( inf, X );
            xSInt64 xi = _mm_srli_epi64( (xUInt64) exponent, 52 );
            x = _mm_or_pd( x, xOne );
            xi = _mm_sub_epi32( xi, bias[1 + isdenorm] );
            exponent = _mm_cvtepi32_pd( (xSInt32) xi );
            
            int j = _mm_cvttsd_si32( _mm_add_sdm( _mm_mul_sdm( x, &d128) , &P1 ));
            xDouble F = _mm_mul_sdm( _mm_cvtsi32_sd( X, j ), &r128 );
            xDouble g = _mm_div_sd( xOne, _mm_add_sd( F, x ) );
            xDouble f = _mm_sub_sd( x, F );
            xDouble a1 = _mm_add_sdm( _mm_mul_sdm( exponent, &logtable[128*2]), &logtable[(j-128)*2] );
            xDouble a2 = _mm_add_sdm( _mm_mul_sdm( exponent, &logtable[128*2]+1), &logtable[(j-128)*2+1] );
            xDouble u = _mm_mul_sd( f, g );
            u = _mm_add_sd( u, u );
            xDouble u1 = _mm_cvtss_sd( u, _mm_cvtsd_ss( (xFloat)u, u ) );
            xDouble v = _mm_mul_sd( u, u );
            xDouble c = _mm_mul_sd( Y, a1 );
            c = _mm_andnot_pd( minusZeroD, c );
            xDouble cLTsixteen = _mm_cmplt_sdm( c, &d16 );
            xDouble cLToneeighth = _mm_cmplt_sdm( c, &r8 );
            xDouble q, u2;
            if( _mm_istrue_sd( cLToneeighth ) )
            {
                //u2 = g * (2 * (f - u1 * F) - u1 * f);
                u2 = _mm_sub_sd( f, _mm_mul_sd( u1, F ) );
                u2 = _mm_add_sd( u2, u2 );
                u2 = _mm_sub_sd( u2, _mm_mul_sd( u1, f ) );
                u2 = _mm_mul_sd( u2, g );

                //q = u * (v * (B1 + v * B2));
                q = _mm_add_sdm( _mm_mul_sdm( v, &B2 ), &B1 );
                q = _mm_mul_sd( q, v );
                q = _mm_mul_sd( q, u );
            }
            else 
            {
                if( _mm_istrue_sd( cLTsixteen ) )
                {
                    //u2 = g * (2 * (f - u1 * F) - u1 * f);
                    u2 = _mm_sub_sd( f, _mm_mul_sd( u1, F ) );
                    u2 = _mm_add_sd( u2, u2 );
                    u2 = _mm_sub_sd( u2, _mm_mul_sd( u1, f ) );
                    u2 = _mm_mul_sd( u2, g );
                
                    //q = u * (v * (A1 + v * (A2 + v * A3)));
                    q = _mm_add_sdm( _mm_mul_sdm( v, &A3 ), &A2 );
                    q = _mm_add_sdm( _mm_mul_sd( q, v ), &A1 );
                    q = _mm_mul_sd( _mm_mul_sd( q, v ), u );
                }
                else
                {
                    xDouble f1 = _mm_cvtss_sd( f, _mm_cvtsd_ss( (xFloat) f, f ));
                    xDouble f2 = _mm_sub_sd( f, f1 );
                
                    //u2 = g * ((2 * (f - u1 * F) - u1 * f1) - u1 * f2);
                    u2 = _mm_sub_sd( f, _mm_mul_sd( u1, F ) );              //f - u1*f
                    u2 = _mm_add_sd( u2, u2 );                              //2*(f - u1*f)
                    u2 = _mm_sub_sd( u2, _mm_mul_sd( u1, f1 ) );            //2*(f - u1*f) - u1*f1
                    u2 = _mm_sub_sd( u2, _mm_mul_sd( u1, f2 ) );            //(2*(f - u1*f) - u1*f1) - u1*f2
                    u2 = _mm_mul_sd( u2, g );                               //g * ((2*(f - u1*f) - u1*f1) - u1*f2)

                    //q = u * (v * (A1 + v * (A2 + v * A3)));
                    q = _mm_add_sdm( _mm_mul_sdm( v, &A3 ), &A2 );
                    q = _mm_add_sdm( _mm_mul_sd( q, v ), &A1 );
                    q = _mm_mul_sd( _mm_mul_sd( q, v ), u );
                }
            }
        
            xDouble p = _mm_add_sd( u2, q );
            xDouble t = _mm_add_sd( a1, u1 );
            a2 = _mm_add_sd( a2, _mm_add_sd( _mm_sub_sd( a1, t), u1 ) );
            z2 = _mm_add_sd( p, a2 );
            z1 = _mm_add_sd( t, z2 );
            z1 = _mm_cvtss_sd( z1, _mm_cvtsd_ss( (xFloat) z1, z1 ));
            z2 = _mm_add_sd( z2, _mm_sub_sd( t, z1) );
        }
        
        /* Procedure M */
        {
            xDouble y1 = _mm_cvtss_sd( Y, _mm_cvtsd_ss( (xFloat) Y, Y )); 
            xDouble y2 = _mm_sub_sd( Y, y1 );
            w2 = _mm_add_sd( _mm_mul_sd( y2, z1 ), _mm_mul_sd( y2, z2 ));
            w2 = _mm_add_sd( w2, _mm_mul_sd( y1, z2 ) );
            w1 = _mm_mul_sd( y1, z1 ); 
        }

        //deal with overflow/underflow
        xDouble overflow = _mm_add_sd( w1, w2);
        xDouble underflow = _mm_cmplt_sdm( overflow, &Emin );
        overflow = _mm_cmpgt_sdm( overflow, &Emax );
        if( _mm_istrue_sd( _mm_or_pd( overflow, underflow ) ) )
        {
            int overflowFlag = _mm_cvtsi128_si32( (xUInt64) overflow ) & OVERFLOW_FLAG;
            int underflowFlag = _mm_cvtsi128_si32( (xUInt64) underflow ) & UNDERFLOW_FLAG;
            oldMXCSR |= overflowFlag | underflowFlag | INEXACT_FLAG;
            X = _mm_and_pd( _mm_load_sd( &plusinf ), overflow );
            goto exit;
        }

		/* Procedure E */
		{
            int N = _mm_cvtsd_si32( _mm_mul_sdm( w1, &Inv_L) ); 
            int j = N & 31;
            int M = N >> 5;
            xDouble dN = _mm_cvtsi32_sd( w1, N );
            xDouble r = _mm_sub_sd( w1, _mm_mul_sdm( dN, &L1 ) );
			r = _mm_add_sd( r, w2 );
            r = _mm_sub_sd( r, _mm_mul_sdm( dN, &L2 ) );
            //double p = r + r * (r * (P1 + r * (P2 + r * (P3 + r * (P4 + r * P5)))));
            xDouble p = _mm_add_sdm( _mm_mul_sdm( r, &P5 ), &P4 );
            p = _mm_add_sdm( _mm_mul_sd( p, r ), &P3 );
            p = _mm_add_sdm( _mm_mul_sd( p, r ), &P2 );
            p = _mm_add_sdm( _mm_mul_sd( p, r ), &P1 );
            p = _mm_add_sd( _mm_mul_sd( _mm_mul_sd( p, r ), r), r );
            //double S = exptable[j].lead + exptable[j].trail;
            xDouble S = _mm_add_sdm( _mm_load_sd( &exptable[ 2 * j ] ), &exptable[ 2 * j +1] );
            xDouble t = _mm_add_sdm( _mm_mul_sd( S, p ), &exptable[ 2 * j +1] );
            t = _mm_add_sdm( t, &exptable[ 2 * j ] );
            X  = xscalb(t, M);

            oldMXCSR |= _mm_getcsr() & ALL_FLAGS;
            goto exit;
		}
    }

    //Handle special cases
    

    xDouble yIsOddInt = _mm_andnot_pd( yIsEven, yIsInt );
    xDouble resultSign = _mm_and_pd( X, minusZeroD );

    //take care of the x == 0 cases
    if( _mm_istrue_sd( xEQzero ) )
    {
        //The x==1,NaN and y == 0, NaN cases have been weeded out above
        int div_zero_flag = _mm_cvtsi128_si32( (xSInt64) yLTzero ) & DIVIDE_0_FLAG;

        //result is Inf if y <0, otherwise result is 0
        X = _mm_and_pd( _mm_load_sd( &plusinf), yLTzero );      

        //only apply the sign only if y is a odd integer
        resultSign = _mm_and_pd( yIsOddInt, resultSign ); 
        X = _mm_or_pd( X, resultSign );
        
        oldMXCSR |= div_zero_flag;
        goto exit;
    }
    
    //take care of the |x| == inf cases
    if( _mm_istrue_sd( _mm_cmpeq_sdm( fabsX, &plusinf ) ) )
    {        
        //result is Inf if y > 0, otherwise result is 0
        X = _mm_andnot_pd( yLTzero, _mm_load_sd( &plusinf) );      

        //only apply the sign only if y is a odd integer
        resultSign = _mm_and_pd( yIsOddInt, resultSign ); 
        X = _mm_or_pd( X, resultSign );
    
        goto exit;
    }

    //take care of the |y| == inf cases
    if( _mm_istrue_sd( fabsyIsInf ) )
    {        
        xDouble fabsxGTone = _mm_cmpgt_sdm( fabsX, &one );
        xDouble isInf = _mm_xor_pd( yLTzero, fabsxGTone );
        xDouble isOne = _mm_cmpeq_sdm( X, &mOne );          // x == -1, |y| == inf returns 1
        X = _mm_and_pd( isInf, _mm_load_sd( &plusinf ) );
        X = _mm_sel_pd( X, _mm_load_sd( &one ), isOne );
        X = _mm_xor_pd( X, signPatch );

        goto exit;
    }


    //We have already handled the following cases:
    //  y = +Inf, -Inf, +0, -0, NaN,
    //  x = +Inf, -Inf, +0, -0, NaN,
    //
    //  x is any and y is a small integer
    //  x is negative and y is a large integer, |y| < 1e20   
    //  x positive finite and  |y| > 1e-20 && |y| < 1e20
    //  x is negative and y is not an integer
    //
    //  We have left:
    //      x is positive finite and |Y| is <= 1e-20
    //      x is positive finite and |Y| is >= 1e20
    //      x is negative finite and |Y| is >= 1e20 and an integer  (we set x = fabs(x) above in this case, |Y| > 1e20 is always even)
    //      x is negative finite and |Y| is <= 1e-20 and an integer (which never happens)
    //
    //  we can be sure that X and Y are not inf, -inf, NaN, 0, -0 at this point,
    //  since those cases are handled, therefore both X and Y are finite
    //
    //  The cases:
    //              y > 1e20    y < 1e-20   y< -1e20
    //  x > 1       inf oi      1.0 i       0.0 ui
    //  x < 1       0.0 ui      1.0 i       inf oi

    xDouble fabsxLTone = _mm_cmplt_sdm( fabsX, &one );
    xDouble underflow = _mm_xor_pd( fabsxLTone, yLTzero );
    xDouble overflow = _mm_andnot_pd( underflow, xLTinf );      //overflow = ~underflow
    xDouble fabsyLTone = _mm_cmplt_sdm( fabsY, &one );
    xDouble fabsxEQone = _mm_cmpeq_sdm( fabsX, &one );
    underflow = _mm_andnot_pd( fabsyLTone, underflow );
    overflow = _mm_andnot_pd( fabsyLTone, overflow );
    int overflowMask = _mm_cvtsi128_si32( (xUInt32) overflow ) & OVERFLOW_FLAG;
    int underflowMask = _mm_cvtsi128_si32( (xUInt32) underflow ) & UNDERFLOW_FLAG;
	int inexactMask = (~ _mm_cvtsi128_si32( (xUInt32) fabsxEQone )) & INEXACT_FLAG;
    X = _mm_and_pd( overflow, _mm_load_sd( &plusinf ) );
    X = _mm_sel_pd( X, _mm_load_sd( &one), fabsyLTone );
    oldMXCSR |= overflowMask | underflowMask | inexactMask;
    _mm_setcsr( oldMXCSR );
    return X;

exit:
    _mm_setcsr( oldMXCSR );
    return _mm_xor_pd( X, signPatch );
} 


double pow( double x, double y )
{
    xDouble xd = DOUBLE_2_XDOUBLE( x );
    xDouble yd = DOUBLE_2_XDOUBLE( y );
    xd = _pow( xd, yd );
    return XDOUBLE_2_DOUBLE( xd );
}

float powf( float x, float y )
{
    xDouble xd = FLOAT_2_XDOUBLE( x );
    xDouble yd = FLOAT_2_XDOUBLE( y );
    xd = _pow( xd, yd );
    return XDOUBLE_2_FLOAT( xd );
}

#include <math.h>
#include <float.h>

//
//	This is not the worlds best power function.  It is merely a simple one that 
//	gives pretty good accuracy, with better range and precision than the double precision
//	one above. It should be off by no more than a few ulps, depending on the accuracy of 
//	exp2 and log2, which are in turn depedent on the hardware fyl2x and .
//
//	Since this is a long double implementation, I have chosen to be more conservative
//	about the level license I take with the C99 spec. It is rigorous about it's flags. 
//	It is perhaps a bit too rigorous about the inexact flag, which we are allowed to abose. 
//	It could be sped up by replacing nearbyintl with floor or round as appropriate.
//
long double powl( long double x, long double y )
{
	static const double neg_epsilon = 0x1.0p63;

	//if x = 1, return x for any y, even NaN
	if( x == 1.0 )
		return x;
	
	//if y == 0, return 1 for any x, even NaN
	if( y == 0.0 )
		return 1.0L;

	//get NaNs out of the way
	if( x != x  || y != y )
		return x + y;
	
	//do the work required to sort out edge cases
	long double fabsy = __builtin_fabsl( y );
	long double fabsx = __builtin_fabsl( x );
	long double infinity = __builtin_infl();
	long double iy = nearbyintl( fabsy );			//we do round to nearest here so that |fy| <= 0.5
	if( iy > fabsy )//convert nearbyint to floor
		iy -= 1.0L;
	int isOddInt = 0;
	if( fabsy == iy && fabsy != infinity && iy < neg_epsilon )
		isOddInt = 	iy - 2.0L * nearbyintl( 0.5L * iy );		//might be 0, -1, or 1
			
	///test a few more edge cases
	//deal with x == 0 cases
	if( x == 0.0 )
	{
		if( ! isOddInt )
			x = 0.0L;

		if( y < 0 )
			x = 1.0L/ x;

		return x;
	}
	
	//x == +-Inf cases
	if( fabsx == infinity )
	{
		if( x < 0 )
		{
			if( isOddInt )
			{
				if( y < 0 )
					return -0.0;
				else
					return -infinity;
			}
			else
			{
				if( y < 0 )
					return 0.0;
				else
					return infinity;
			}
		}

		if( y < 0 )
			return 0;
		return infinity;
	}

	//y = +-inf cases
	if( fabsy == infinity )
	{
		if( x == -1 )
			return 1;

		if( y < 0 )
		{
			if( fabsx < 1 )
				return infinity;
			return 0;
		}
		if( fabsx < 1 )
			return 0;
		return infinity;
	}

	// x < 0 and y non integer case
	if( x < 0 && iy != fabsy )
		return nanl("37") + sqrtl( -1 );
	
	//speedy resolution of sqrt and reciprocal sqrt
	if( fabsy == 0.5 )
	{
		x = sqrtl( x );
		if( y < 0 )
			x = 1.0L/ x;
		return x;
	}

	//Enter the main power function.  This is done as:
	//
	//		split up x**y as:
	//
	//			x**y	= x**(i+f)		i = integer part of y, f = positive fractional part
	//					= x**f * x**i
	//
	long double fy = fabsy - iy;
	
	long double fx = 1.0;
	long double ix = 1.0;

	//Calculate fx = x**f
	if( fy != 0 ) //This is expensive and may set unwanted flags. skip if unneeded
	{
		fx =log2l(x);

		long double fabsfx = __builtin_fabsl( fx );
		long double min = fminl( fy, fabsfx );
		long double max = fmaxl( fy, fabsfx );

		if( y < 0  )
			fy = -fy;

		//if fx * fy is a denormal, we get spurious underflow here, so try to avoid that
		if( min < 0x1.0p-8191L && max < neg_epsilon )	//a crude test for a denormal product
		{
			fx = 1;		//for small numbers, skip straight to the result
		}
		else
		{ //safe to do the work
			fx *= fy;
			fx = exp2l( fx );
		}
	}

	//calculate ix = f**i
	
	//if y is negative, we will need to take the reciprocal eventually
	//Do it now to avoid underflow when we should get overflow
	//but don't do it if iy is zero to avoid spurious overflow
	if( y < 0 && iy != 0 )
		x = 1.0L/ x;

	//calculate x**i by doing lots of multiplication
	while( iy != 0.0L )
	{
		long double ylo;

		//early exit for underflow and overflow. Otherwise we may end up looping up to 16383 times here.
		if( x == 0.0 || x == infinity )
		{
			ix *= x;	//we know this is the right thing to do, because iy != 0
			break;
		}

		//chop off 30 bits at a time
		if( iy > 0x1.0p30 )
		{
			long double scaled = iy * 0x1.0p-30L;
			long double yhi = nearbyintl( scaled );
			if( yhi > scaled )
				yhi -= 1.0;
			ylo = iy - 0x1.0p30L * yhi;
			iy = yhi;
		}
		else
		{ //faster code for the common case
			ylo = iy;
			iy = 0;
		}

		int j;
		int i = ylo;
		int mask = 1;

		//for each of the 30 bits set in i, multiply ix by x**(2**bit_position)
		if( i & 1 )
		{
			ix *= x;
			i -= mask;
		}
		for( j = 0; j < 30 && i != 0; j++ )
		{
			mask += mask;
			x *= x;
			if( i & mask )
			{
				ix *= x;
				i -= mask;
			}
		}
		
		//we may have exited early from the loop above. 
		//If so, and there are still bits in iy finish out the multiplies
		if( 0.0 != iy )
			for( ; j < 30; j++ )
				x *= x;
	}

	x = fx * ix;

	return x;
}



#pragma mark -
#pragma mark cbrt

static inline double _cbrt( double _x ) ALWAYS_INLINE;

double _cbrt( double _x )
{
    static const double infinity = __builtin_inf();
    static const double smallestNormal = 0x1.0p-1022;
    static const double twom968 = 0x1.0p-968;
    static const xUInt64 oneThird = { 0x55555556ULL, 0 };
    static const xUInt32 denormBias = { 0, 696219795U, 0, 0 };
    static const xUInt32 normalBias = { 0, 0x1200000U, 0, 0 };

    static const double C =  5.42857142857142815906e-01; /* 19/35     = 0x3FE15F15, 0xF15F15F1 */
    static const double D = -7.05306122448979611050e-01; /* -864/1225 = 0xBFE691DE, 0x2532C834 */
    static const double E =  1.41428571428571436819e+00; /* 99/70     = 0x3FF6A0EA, 0x0EA0EA0F */
    static const double F =  1.60714285714285720630e+00; /* 45/28     = 0x3FF9B6DB, 0x6DB6DB6E */
    static const double G =  3.57142857142857150787e-01; /* 5/14      = 0x3FD6DB6D, 0xB6DB6DB7 */

    static const xDouble expMask = { __builtin_inf(), 0 };
    static const xUInt64 topMask = { 0xFFFFFFFF00000000ULL, 0 };
    static const double twom20 = 0x1.0p-20;

	xDouble x = DOUBLE_2_XDOUBLE( _x ); 

	if( EXPECT_FALSE( _mm_istrue_sd( _mm_cmpunord_sd( x, x ) ) ) )
		return _x + _x;

	xDouble sign = _mm_and_pd( x, minusZeroD );     //set aside sign
	x = _mm_andnot_pd( minusZeroD, x );             //x = fabs(x)

    if( EXPECT_FALSE( _mm_istrue_sd( _mm_cmpeq_sdm( x, &infinity)) ) )
		return _x;

	if( EXPECT_TRUE( _x != 0.0 ) )
	{
		xDouble isDenorm = _mm_cmplt_sdm( x, &smallestNormal );
		xDouble t = _mm_and_pd( _mm_load_sd( &twom968 ), isDenorm );        //t is 0 for normals, and 2**54 for denormals
		t = _mm_sub_sd( _mm_or_pd( t, x ), t );                             //multiply of t * x for denormals. Exact for normals.
		
		//prepare cheesy estimate of t**1/3 by dividing high 32-bits by 3
		xSInt32 addend = _mm_add_epi32( _mm_andnot_si128( (xSInt32) isDenorm, normalBias), denormBias );
		t = (xDouble) _mm_mul_epu32( _mm_srli_epi64( (xSInt64) t, 32 ), oneThird );
		t = (xDouble) _mm_add_epi32( (xSInt32) t, addend );
		
		//new cbrt to a reported 23 bits (may be implemented in single precision
		xDouble r = _mm_div_sd( _mm_mul_sd( t, t ), x );        //r = t*t/x
		xDouble s = _mm_add_sdm( _mm_mul_sd( r, t ), &C );      //s = C + r*t
		xDouble y = _mm_add_sdm( s, &E );
		xDouble z = _mm_div_sd( _mm_load_sd( &D ), s );
		y = _mm_add_sd( y, z );
		y = _mm_div_sd( _mm_load_sd( &F ), y );
		y = _mm_add_sdm( y, &G );
		t = _mm_mul_sd( t, y );

		/* chopped to 20 bits and make it larger than cbrt(x) */ 
		xDouble add = _mm_mul_sdm( t, &twom20 ); 
		add = _mm_and_pd( add, expMask );
		t = _mm_add_sd( t, add );
		t = _mm_and_pd( t, (xDouble) topMask );
		
		/* one step newton iteration to 53 bits with error less than 0.667 ulps */
		s = _mm_mul_sd( t, t );            //exact
		r = _mm_div_sd( x, s );         
		xDouble w = _mm_add_sd( t, t );
		w = _mm_add_sd( w, r );
		r = _mm_sub_sd( r, t );
		r = _mm_div_sd( r, w );
		t = _mm_add_sd( t, _mm_mul_sd( t, r ) );
		
		//restore sign
		t = _mm_or_pd( t, sign );
	
		return XDOUBLE_2_DOUBLE( t );
	}

	return _x;      //result for 0.0 and -0.0
}

double cbrt( double x )
{
	return _cbrt( x );
}

float cbrtf( float x )
{
    return _cbrt(x);
}


#endif /* defined( __i386__ ) */
