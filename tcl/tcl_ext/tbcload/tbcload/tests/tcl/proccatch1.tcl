# proccatch1.tcl --
#
#  Test file for compilation.
#  This file defines four procs, which are all compiled. It also defines
#  enough variables to push us past 256 objects in the object table.
#  It also has a catch {unset dummy} statement in the middle of the set of
#  variable. The statement should be caught.
#  The file checks for the following:
#  The compiled procs cause a new object to be added to the object table;
#  because this object has an index > 256, it needs 4 bytes of storage in the
#  bytecodes. Thus, cmpWrite.c:UpdateBytecodes pushes the bytecodes to the
#  right. If the bytecodes are pushed enough (2 or more procs), the
#  bytecodes for the "unset dummy" end up outside the exception range for the
#  catch, and the "catch {}" fails to work.
#  The check here is to make sure that UpdateBytecodes updates the exception
#  ranges when the bytecodes are pushed.
#
# Copyright (c) 1998-2000 by Ajuba Solutions.
# All rights reserved.
# 
# RCS: @(#) $Id: proccatch1.tcl,v 1.2 2000/05/30 22:19:12 wart Exp $

# make sure dummy is not defined.
# This catch is safe, because it's done before the procs are defined

catch {unset dummy}

proc a { x } { return "$x : $x" }
proc b { x {y dummy} } { return "$x : $y" }
proc c { z } { return "$z : $z" }
proc d { z {y dummy} } { return "$z : $y" }

# these statements are here to generate enough objects to push us past the
# value of 255 for the next available index, which will be used for the
# "tbcload::bcproc" string object (to replace "proc")

set var0 0
set var1 1
set var2 2
set var3 3
set var4 4
set var5 5
set var6 6
set var7 7
set var8 8
set var9 9
set var10 10
set var11 11
set var12 12
set var13 13
set var14 14
set var15 15
set var16 16
set var17 17
set var18 18
set var19 19
set var20 20
set var21 21
set var22 22
set var23 23
set var24 24
set var25 25
set var26 26
set var27 27
set var28 28
set var29 29
set var30 30
set var31 31
set var32 32
set var33 33
set var34 34
set var35 35
set var36 36
set var37 37
set var38 38
set var39 39
set var40 40
set var41 41
set var42 42
set var43 43
set var44 44
set var45 45
set var46 46
set var47 47
set var48 48
set var49 49
set var50 50
set var51 51
set var52 52
set var53 53
set var54 54
set var55 55
set var56 56
set var57 57
set var58 58
set var59 59
set var60 60
set var61 61
set var62 62
set var63 63
set var64 64

# this error should be caught by the catch.

catch {unset dummy}

set var65 65
set var66 66
set var67 67
set var68 68
set var69 69
set var70 70
set var71 71
set var72 72
set var73 73
set var74 74
set var75 75
set var76 76
set var77 77
set var78 78
set var79 79
set var80 80
set var81 81
set var82 82
set var83 83
set var84 84
set var85 85
set var86 86
set var87 87
set var88 88
set var89 89
set var90 90
set var91 91
set var92 92
set var93 93
set var94 94
set var95 95
set var96 96
set var97 97
set var98 98
set var99 99
set var100 100
set var101 101
set var102 102
set var103 103
set var104 104
set var105 105
set var106 106
set var107 107
set var108 108
set var109 109
set var110 110
set var111 111
set var112 112
set var113 113
set var114 114
set var115 115
set var116 116
set var117 117
set var118 118
set var119 119
set var120 120
set var121 121
set var122 122
set var123 123
set var124 124
set var125 125
set var126 126
set var127 127
set var128 128
set var129 129
set var130 130
set var131 131
set var132 132
set var133 133
set var134 134
set var135 135
set var136 136
set var137 137
set var138 138
set var139 139
set var140 140
set var141 141
set var142 142
set var143 143
set var144 144
set var145 145
set var146 146
set var147 147
set var148 148
set var149 149
set var150 150
set var151 151
set var152 152
set var153 153
set var154 154
set var155 155
set var156 156
set var157 157
set var158 158
set var159 159
set var160 160
set var161 161
set var162 162
set var163 163
set var164 164
set var165 165
set var166 166
set var167 167
set var168 168
set var169 169
set var170 170
set var171 171
set var172 172
set var173 173
set var174 174
set var175 175
set var176 176
set var177 177
set var178 178
set var179 179
set var180 180
set var181 181
set var182 182
set var183 183
set var184 184
set var185 185
set var186 186
set var187 187
set var188 188
set var189 189
set var190 190
set var191 191
set var192 192
set var193 193
set var194 194
set var195 195
set var196 196
set var197 197
set var198 198
set var199 199
set var200 200
set var201 201
set var202 202
set var203 203
set var204 204
set var205 205
set var206 206
set var207 207
set var208 208
set var209 209
set var210 210
set var211 211
set var212 212
set var213 213
set var214 214
set var215 215
set var216 216
set var217 217
set var218 218
set var219 219
set var220 220
set var221 221
set var222 222
set var223 223
set var224 224
set var225 225
set var226 226
set var227 227
set var228 228
set var229 229
set var230 230
set var231 231
set var232 232
set var233 233
set var234 234
set var235 235
set var236 236
set var237 237
set var238 238
set var239 239
set var240 240
set var241 241
set var242 242
set var243 243
set var244 244
set var245 245
set var246 246
set var247 247
set var248 248
set var249 249
set var250 250
set var251 251
set var252 252
set var253 253
set var254 254
set var255 255
set var256 256
set var257 257
set var258 258
set var259 259
set var260 260
set var261 261
set var262 262
set var263 263
set var264 264
set var265 265
set var266 266
set var267 267
set var268 268
set var269 269
set var270 270
set var271 271
set var272 272
set var273 273
set var274 274
set var275 275
set var276 276
set var277 277
set var278 278
set var279 279
set var280 280
set var281 281
set var282 282
set var283 283
set var284 284
set var285 285
set var286 286
set var287 287
set var288 288
set var289 289
set var290 290
set var291 291
set var292 292
set var293 293
set var294 294
set var295 295
set var296 296
set var297 297
set var298 298
set var299 299

set result TEST
