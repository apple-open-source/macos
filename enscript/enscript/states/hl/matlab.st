/**
 * Name: matlab
 * Description: Matlab programming language.
 * Author: Jack Dunn <h.j.dunn@larc.nasa.gov>
 */

m_keyword_re =
/\b(break|cl(ear|ose)|e(nd|lse|lseif|rror)|f(or|unction)|if|keyboard|otherwise\
|quit|return|switch|while)\b/;

/* Taken fron the Matlab Function Reference, version 5.2 */
m_type_re =
/\b(abs|acopy|acos|acosh|acot|acoth|acsc|acsch|addpath|airy|all|and|angle|ans|any\
|arc(cosecant|cosine|cotangent|secant|sine|tangent)|area\
|arename|asech|asin|asinh|atan(|2)|atanh|axes|axis|balance|bar(|3|3h|h)\
|base2dec|bessel(h|i|j|k|y)|beta(|inc|ln)\
|bicgstab|bin2dec|bit(and|cmp|get|max|or|set|shift|xor)|blanks\
|box|brighten|builtin|calendar|cam(dolly|light|lookat|orbit|pan|pos|proj\
|roll|target|up|va|zoom)|capture|case|cat(|ch)|caxis|cd|cdf2rdf\
|cell(|2struct|plot)|cgs|char|chol(|inc|update)|cla(|bel|ss)\
|clc|clf|clock|col(mmd|or(bar|map)|perm)|com(bs|et|et3|pa(n|ss))|computer\
|cond(|eig|est)|conj|con(tour(|3|c|f))|contrast|conv(|2)|convhull|conv\
|copy(file|obj)|corrcoef|cos|cosh|cot|coth|cov|cplxpair|cputime|cross\
|csc|csch|cum(prod|trapz)|cylinder|daspect|date|date(num|str|vec)\
|db(clear|cont|down|mex|quit|stack|status|step|stop|type|up)\
|dd(eadv|eexec|einit|epoke|ereq|eterm|eunadv)|de(al|blank|(c2(base|bin|hex))\
|conv)|default4|del2|delaunay|de(lete|t)|diag|diary|diff|dir|disp\
|dlm(read|writ)|dmperm|double|dragrect|drawnow|dsearch|echo|eig|eigs|ellipj\
|ellipke|eomday|eps|erf(|c|cx)|etime|eval|evalin|exist|exp|expint|expm|eye\
|ezplot|factor|fclose|feather|feof|ferror|feval|fft(|2|n|shift)\
|fget(l|s)|fid|figflag|figure|fileparts|fill|fill3|filter(|2)|find(|obj|str)\
|fix|flip(dim|lr|ud)|floor|flops|fmin(|s)|fopen|format|fplot|fprintf\
|frame(2im|edit)|fread|freqspace|frewind|fscanf|fseek|ftell|full|funm|fwrite\
|fzero|gallery|gamma(|inc|ln)|g(ca|cd|cf|co)|gestalt|get(|field|frame)\
|ginput|global|gmres|gplot|gradient|graymon|grid(|data)|gsvd|gtext|hadamard\
|hankel|hdf|help|hess|hex2(dec|num)|hidden|hilb|hist|hold|home|horzcat\
|hsv2rgb|i|ifft(|2|n|shift)|im2frame|imag|image|imagesc|im(finfo|read|write)\
|ind2sub|Inf|inferiorto|inline|inpolygon|input|int2str|interp(1|2|3|ft|n)\
|intersect|inv|invhilb|ipermute|is(a|cell(|str)|char|empty|equal|field\
|finite|global|handle|hold|ieee|inf|letter|logical|member|nan|numeric\
|object|ppc|prime|real|space|sparse|str|struct|student|unix|vms)\
|j|kron|last(err|warn)|lcm|ldivide|legend(|re)|length|light(|angle|ing)\
|lin2mu|line|linspace|load|log(|10|2|ical)|loglog(|m|space)|lookfor\
|lower|lscov|lu|luinc|magic|mat2str|material|max|mean|median|menu\
|mesh(|c|grid|z)|min|minus|mislocked|mkdir|ml(divide|ock)|mod|more|movie(|in)\
|mpower|mrdivide|mtimes|mu2lin|munlock|NaN|narg(chk|in|out)|nd(grid|ims)\
|newplot|nextpow2|nnls|nnz|nonzeros|norm|normest|not|now|null|num2(cell|str)\
|nzmax|ode(45|file|get|set)|ones|or|orient|orth|pack|pagedlg|pareto|pascal\
|patch|path|pause|pbaspect|pcg|pcode|pcolor|perms|permute|persistent\
|pi|pie(|,3)|pinv|plot(|3|matrix|yy)|plus|pol2cart|polar|poly(|area|der\
|eig|fit|val|valm)|pow2|power|primes|print(|dlg|frame|opt)|prod\
|profile|qmr|qr(|delete|insert)|qtwrite|quad|quad8|quiver(|3)|qz\
|rand(|n|perm)|rank|rat(|s)|rbbox|rcond|rdivide|readsnd|real(|max|min)\
|recordsound|refresh|rem|repmat|reset|reshape|residue|rgb(2hsv|plot)\
|ribbon|rmfield|rmpath|roots|rose|rot90|rotate(|3d)|round|rref(|movie)\
|rsf2csf|save|scatter(|3)|schur|script|sech|semilog(x|y)|set(|diff|feild|xor)\
|shading|shiftdim|sign|sin|sinh|size|slice|sliders|sort|sortrows|sound(|cap)\
|sp(alloc|arse|convert|diags|eak|eye|fun|h2cart|here|inmap|line\
|ones|parms|rand|randn|randsym|rintf|y)|sqrt(|m)|squeeze|sscanf|stairs\
|startup|std|stem(|3)|str(2(cell|num)|cat|cmp|cmpi|ings|just|match|ncmp\
|ncmpi|rep|tok|uct2cell,vcat)|sub(2ind|plot|s(asgn|pace|ref))|sum|superiorto\
|surf(|ace|c|l|norm)|svd(|s)|sym(mmd|rcm)|tan|tanh|tempdir|tempname\
|terminal|text|tic|times|title|toc|toeplitz|trace|transpose|trapz\
|tri(l|mesh|surf|u)|try|tsearch|type|ui(control|getfile|menu|nt8|putfile\
|resume|setcolor|setfont)|uminus|union|unique|unwrap|uplus|upper|varargout\
|vectorize|ver(|sion|tcat)|view|viewmtx|voronoi|wait(bar|for(|buttonpress))\
|warndlg|warning|waterfall|wav(read|write)|weekday|what(|snew)|which|who(|s)\
|wilkinson|wk1(read|write)|writesnd|xlabel|xlgetrange|xlim|xlsetrange|xor\
|ylabel|ylim|zeros|zlabel|zlim|zoom\
)\b/;

state m_string extends Highlight
{
  /\\\\./ {
    language_print ($0);
  }
  /[\']/ {
    language_print ($0);
    return;
  }
}

state matlab extends HighlightEntry
{
  /* Comments. */
  /%/ {
    comment_face (true);
    language_print ($0);
    call (eat_one_line);
    comment_face (false);
  }

  /* Continuation. */
  /\.\.\./ {
    keyword_face (true);
    language_print ($0);
    keyword_face (false);
  }

  /* Matrix start.  */
  /\[/ {
    type_face (true);
    language_print ($0);
    type_face (false);
  }

  /* Matrix end. */
  /\]/ {
    type_face (true);
    language_print ($0);
    type_face (false);
  }

  m_type_re {
    type_face (true);
    language_print ($0);
    type_face (false);
  }

  m_keyword_re {
     keyword_face (true);
     language_print($0);
     keyword_face (false);
  }

  /* Transpose. */
  /* variable' or )' */
  /([a-zA-Z][a-zA-Z_0-9]*|\))([\'])/ {
    language_print ($1);
    keyword_face (true);
    language_print ($2);
    keyword_face (false);
  }
  /* ]' */
  /([a-zA-Z][a-zA-Z_0-9]*|])([\'])/ {
    type_face (true);
    language_print ($1);
    type_face (false);
    keyword_face (true);
    language_print ($2);
    keyword_face (false);
  }

  /* Strings. */
  /*  'any number of characters'  */
  /[\']/ {
    string_face (true);
    language_print ($0);
    call (m_string);
    string_face (false);
  }
}


/*
Local variables:
mode: c
End:
*/
