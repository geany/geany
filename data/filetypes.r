# For complete documentation of this file, please see Geany's main documentation
[styling]
# foreground;background;bold;italic
default=0x000000;0xffffff;false;false
comment=0x808080;0xffffff;false;false
# primary keyword
kword=0x00007f;0xffffff;false;false
operator=0x301010;0xffffff;false;false
# package keyword
basekword=0x991111;0xffffff;false;false
# package_other keyword
otherkword=0x991111;0xffffff;false;false
number=0x007f00;0xffffff;false;false
# "blah" string
string=0xff901e;0xffffff;false;false
# 'blah' string
string2=0xff901e;0xffffff;false;false
identifier=0x000000;0xffffff;false;false
infix=0x008000;0xffffff;false;false
infixeol=0x000000;0xe0c0e0;false;false

[keywords]
# all items must be in one line
primary=break else F FALSE for function if in Inf NA NaN next NULL repeat require return source T TRUE while
package=abbreviate abline abs acf acos acosh addmargins aggregate agrep alarm alias alist all anova any aov aperm append apply approx approxfun apropos ar args arima array arrows asin asinh assign assocplot atan atanh attach attr attributes autoload autoloader ave axis backsolve barplot basename beta bindtextdomain binomial biplot bitmap bmp body box boxplot bquote browser builtins bxp by bzfile c call cancor capabilities casefold cat category cbind ccf ceiling character charmatch chartr chol choose chull citation class close cm cmdscale codes coef coefficients col colnames colors colorspaces colours comment complex confint conflicts contour contrasts contributors convolve cophenetic coplot cor cos cosh cov covratio cpgram crossprod cummax cummin cumprod cumsum curve cut cutree cycle data dataentry date dbeta dbinom dcauchy dchisq de debug debugger decompose delay deltat demo dendrapply density deparse deriv det detach determinant deviance dexp df dfbeta dfbetas dffits dgamma dgeom dget dhyper diag diff diffinv difftime digamma dim dimnames dir dirname dist dlnorm dlogis dmultinom dnbinom dnorm dotchart double dpois dput drop dsignrank dt dump dunif duplicated dweibull dwilcox eapply ecdf edit effects eigen emacs embed end environment eval evalq example exists exp expression factanal factor factorial family fft fifo file filter find fitted fivenum fix floor flush for force formals format formula forwardsolve fourfoldplot frame frequency ftable gamma gaussian gc gcinfo gctorture get getenv geterrmessage gettext gettextf getwd gl glm globalenv gray grep grey grid gsub gzcon gzfile hat hatvalues hcl hclust head heatmap help hist history hsv httpclient iconv iconvlist identical identify if ifelse image influence inherits integer integrate interaction interactive intersect invisible isoreg jitter jpeg julian kappa kernapply kernel kmeans knots kronecker ksmooth labels lag lapply layout lbeta lchoose lcm legend length letters levels lfactorial lgamma library licence license line lines list lm load loadhistory loadings local locator loess log logb logical loglin lowess ls lsfit machine mad mahalanobis makepredictcall manova mapply match matlines matplot matpoints matrix max mean median medpolish menu merge message methods mget min missing mode monthplot months mosaicplot mtext mvfft names napredict naprint naresid nargs nchar ncol nextn ngettext nlevels nlm nls noquote nrow numeric objects offset open optim optimise optimize options order ordered outer pacf page pairlist pairs palette par parse paste pbeta pbinom pbirthday pcauchy pchisq pdf pentagamma person persp pexp pf pgamma pgeom phyper pi pico pictex pie piechart pipe plclust plnorm plogis plot pmatch pmax pmin pnbinom png pnorm points poisson poly polygon polym polyroot postscript power ppoints ppois ppr prcomp predict preplot pretty princomp print prmatrix prod profile profiler proj promax prompt provide psigamma psignrank pt ptukey punif pweibull pwilcox q qbeta qbinom qbirthday qcauchy qchisq qexp qf qgamma qgeom qhyper qlnorm qlogis qnbinom qnorm qpois qqline qqnorm qqplot qr qsignrank qt qtukey quantile quarters quasi quasibinomial quasipoisson quit qunif quote qweibull qwilcox rainbow range rank raw rbeta rbind rbinom rcauchy rchisq readline real recover rect reformulate regexpr relevel remove reorder rep replace replicate replications reshape resid residuals restart rev rexp rf rgamma rgb rgeom rhyper rle rlnorm rlogis rm rmultinom rnbinom rnorm round row rownames rowsum rpois rsignrank rstandard rstudent rt rug runif runmed rweibull rwilcox sample sapply save savehistory scale scan screen screeplot sd search searchpaths seek segments seq sequence serialize setdiff setequal setwd shell sign signif sin single sinh sink smooth solve sort spectrum spline splinefun split sprintf sqrt stack stars start stderr stdin stdout stem step stepfun stl stop stopifnot str strftime strheight stripchart strptime strsplit strtrim structure strwidth strwrap sub subset substitute substr substring sum summary sunflowerplot supsmu svd sweep switch symbols symnum system table tabulate tail tan tanh tapply tempdir tempfile termplot terms tetragamma text time title toeplitz tolower topenv toupper trace traceback transform trigamma trunc truncate try ts tsdiag tsp typeof unclass undebug union unique uniroot unix unlink unlist unname unserialize unsplit unstack untrace unz update upgrade url var varimax vcov vector version vi vignette warning warnings weekdays weights which window windows with write wsbrowser xedit xemacs xfig xinch xor xtabs xyinch yinch zapsmall
package_other=acme aids aircondit amis aml banking barchart barley beaver bigcity boot brambles breslow bs bwplot calcium cane capability cav censboot channing city claridge cloth cloud coal condense contourplot control corr darwin densityplot dogs dotplot ducks empinf envelope environmental ethanol fir frets gpar grav gravity grob hirose histogram islay knn larrows levelplot llines logit lpoints lsegments lset ltext lvqinit lvqtest manaus melanoma melanoma motor multiedit neuro nitrofen nodal ns nuclear oneway parallel paulsen poisons polar qq qqmath remission rfs saddle salinity shingle simplex singer somgrid splom stripplot survival tau tmd tsboot tuna unit urine viewport wireframe wool xyplot

[settings]
# default extension used when saving files
#extension=R

# the following characters are these which a "word" can contains, see documentation
#wordchars=_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789

# if only single comment char is supported like # in this file, leave comment_close blank
comment_open=#
comment_close=
# this is an alternative way, so multiline comments are used
#comment_open=/*
#comment_close=*/

# set to false if a comment character/string should start at column 0 of a line, true uses any
# indentation of the line, e.g. setting to true causes the following on pressing CTRL+d
   #command_example();
# setting to false would generate this
#  command_example();
# This setting works only for single line comments
comment_use_indent=false

# context action command (please see Geany's main documentation for details)
context_action_cmd=

