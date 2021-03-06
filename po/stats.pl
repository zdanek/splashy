#!/usr/bin/perl
# Modified by Luis Mondesi <lemsx1@gmail.com> for Splashy
# Copyright 2003-2004 Nathan Walp <faceprint@faceprint.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 50 Temple Place, Suite 330, Boston, MA 02111-1307  USA
#

my $PACKAGE = "splashy";

use Locale::Language;

$lang{en_AU}     = "English (Australian)";
$lang{en_CA}     = "English (Canadian)";
$lang{en_GB}     = "English (British)";
$lang{pt_BR}     = "Portuguese (Brazilian)";
$lang{'sr@Latn'} = "Serbian (Latin)";
$lang{zh_CN}     = "Chinese (Simplified)";
$lang{zh_TW}     = "Chinese (Traditional)";

opendir(DIR, ".") || die "can't open directory: $!";
@pos = grep { /\.po$/ && -f } readdir(DIR);
foreach (@pos) { s/\.po$//; }
closedir DIR;

@pos = sort @pos;

$now = `date`;

system("./update.pl --pot > /dev/null");

$_ = `msgfmt --statistics $PACKAGE.pot -o /dev/null 2>&1`;

die "unable to get total: $!" unless (/(\d+) untranslated messages/);

$total = $1;

print "<html>\n";
print "<head><title>$PACKAGE i18n statistics</title><style>th { color: white; }</style></head>\n";
print "<body>\n";
print
  "<table cellspacing='0' cellpadding='0' border='0' bgcolor='#888888' width='100%'><tr><td><table cellspacing='1' cellpadding='2' border='0' width='100%'>\n";

print
  "<tr bgcolor='#c0c0c0'><th>language</th><th style='background: #339933;'>trans</th><th style='background: #339933;'>%</th><th style='background: #333399;'>fuzzy</th><th style='background: #333399;'>%</th><th style='background: #dd3333;'>untrans</th><th style='background: #dd3333;'>%</th><th>&nbsp;</th></tr>\n";

foreach $index (0 .. $#pos)
{
    $trans = $fuzz = $untrans = 0;
    $po = $pos[$index];
    print STDERR "$po..." if ($ARGV[0] eq '-v');
    system("msgmerge $po.po $PACKAGE.pot -o $po.new 2>/dev/null");
    $_ = `msgfmt --statistics $po.new -o /dev/null 2>&1`;
    chomp;
    if (/(\d+) translated message/)   { $trans   = $1; }
    if (/(\d+) fuzzy translation/)    { $fuzz    = $1; }
    if (/(\d+) untranslated message/) { $untrans = $1; }
    $transp   = 100 * $trans / $total;
    $fuzzp    = 100 * $fuzz / $total;
    $untransp = 100 * $untrans / $total;

    if ($index % 2)
    {
        $color = " bgcolor='#e0e0e0'";
    }
    else
    {
        $color = " bgcolor='#d0e0ff'";
    }
    $name = "";
    $name = $lang{$po};
    $name = code2language($po) unless $name ne "";
    $name = "???" unless $name ne "";
    printf
      "<tr$color><td>%s(<a href='http://git.debian.org/?p=splashy/splashy.git;a=blob_plain;f=po/%s.po;hb=HEAD'>%s.po</a>)</td><td>%d</td><td>%0.2f</td><td>%d</td><td>%0.2f</td><td>%d</td><td>%0.2f</td><td>",
      $name, $po, $po, $trans, $transp, $fuzz, $fuzzp, $untrans, $untransp;
    printf "<img src='bar_g.png' height='15' width='%0.0f' />", $transp * 2
      unless $transp * 2 < 0.5;
    printf "<img src='bar_b.png' height='15' width='%0.0f' />", $fuzzp * 2
      unless $fuzzp * 2 < 0.5;
    printf "<img src='bar_r.png' height='15' width='%0.0f' />", $untransp * 2
      unless $untransp * 2 < 0.5;
    print "</tr>\n";
    unlink("$po.new");
    print STDERR "done ($untrans untranslated strings).\n"
      if ($ARGV[0] eq '-v');
}
print "</table></td></tr></table>\n";
print
  "Latest $PACKAGE.pot generated $now: <a href='http://git.debian.org/?p=splashy/splashy.git;a=blob_plain;f=po/splashy.pot;hb=HEAD'>$PACKAGE.pot</a><br />\n";
print
  "Use <a href='http://git.debian.org/?p=splashy/splashy.git;a=blob_plain;f=po/check_po.pl;hb=HEAD'>check_po.pl</a> to ensure your po file works. Example: <b>./check_po.pl foo.po</b>.<br />$PACKAGE.pot must exist where foo.po is.<br />\n";
print "</body>\n";
print "</html>\n";

