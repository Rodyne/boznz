# boznz

This is Boz's personal site: a short home page, a blog, an about page, and pages for the novels. It used to be a WordPress site at [rodyne.com](https://rodyne.com). It is now ordinary HTML files, so any simple web host can serve it. There is no database, no build step, and no WordPress.

Hosting for boznz.com is not set up yet. Do not add a CNAME file until the domain is ready.

## How the files are organised

Everything you open in a browser sits in this folder:

- `index.html` is the home page.
- `post1234.html` is a blog post or a standalone page. The number is the old WordPress id. Contact is `post271.html`. About is `post286.html`. The Summer of Reasoning page is `post1252.html`.
- `category-12.html` and `tag-some-topic.html` are the old category and tag listings. If a listing is long, the next page is `category-12-page-2.html` (tags and the author listing work the same way).
- `author-2.html` lists posts by the site's author.
- `images/` holds the pictures. `files/` holds PDFs, zip files, and other downloads.
- `css/site.css` is the stylesheet.
- `url-map.csv` is the old-address to new-file list. Each row is an old rodyne.com URL, the new filename, what kind of page it is, and the title.

`www.rodyne.com` served the same pages as `rodyne.com`. Both are in the URL map, and both point at `index.html` for the home page.

## Preview it on your computer

Python is enough. It is already installed on a Mac and on most Linux computers.

1. Open a terminal in this folder.
2. Run `python3 -m http.server 8000`
3. In a browser, open http://localhost:8000

You can also double-click `index.html`. Links from one page to another work either way.

## What was removed

These needed a live server, so they are not in this copy:

- Comments. The exported pages had no comments on them, only empty comment areas and a "comments off" line.
- The view counter in the sidebar.
- RSS feeds, and the RSS link in the sidebar.
- WordPress extras that do not affect the page: emoji scripts, oEmbed and REST links, xmlrpc, pingback, generator tags, and the extra image `srcset` sizes.

The contact page is still the email address and phone number. There was no contact form. There was no search box.

## Stylesheet

The live pages ask for the Twenty Ten theme stylesheet, but that file currently returns "not found" on rodyne.com, so the live site looks unstyled. This copy includes the site's own theme file (saved from the Internet Archive, the `style.css?ver=20260520` the pages were requesting, plus `blocks.css`). That is what draws the header, the column, and the sidebar.

## Files that were already missing

These links were already broken on rodyne.com. The pages now say so in place, instead of pointing at a dead address:

- `shwd2-1024x768.jpg`, `shed3.jpg`, and `shed4.jpg` on the shed post (`post1045.html`)
- `terminal-case.png` on `post661.html`
- `ChainDisplay.zip` on `post1097.html`
- Two old "attachment 780" links in `post777.html` that never pointed at a real file

One outside link also 404s: the Barnes & Noble NOOK help article linked from a book post. It is left as-is because it is not part of this site.

## The original download

The untouched download of rodyne.com is still in git history, in the commit "Raw mirror of rodyne.com", if you ever need to go back to it. It is not in the folder you serve.
