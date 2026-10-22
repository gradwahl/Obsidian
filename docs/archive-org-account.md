# Archive.org account access (browser-first, initial implementation)

Obsidian's AlvRo source indexes **eight** Archive.org collections. Most entries
are marked private by Archive.org metadata even though an account may be able
to view or download them. This is **not** an OAuth or embedded Archive.org
login integration.

## Using it

1. In Obsidian's **Settings**, find **Archive.org account**.
2. Choose **Sign in at Archive.org**. The link opens
   `https://archive.org/account/login.php` on Archive.org itself.
3. After completing sign-in there, return to Obsidian and open one of the
   eight AlvRo collection directories in the **same browser**.
4. In an indexed AlvRo game detail screen, **Browse collection on Archive.org**
   opens that game's verified collection. Download options and authorisation
   are controlled by Archive.org, not Obsidian.

Obsidian does **not** ask for or store Archive.org passwords, cookies, S3 keys,
tokens, or account identity. No account status is inferred; clicking sign-in
does not mean Obsidian has verified access, and Obsidian does not proxy
restricted files or request a cross-origin session.

A browser's authenticated cookies are attached by the browser itself when
visiting Archive.org as a first-party site. Opening the sign-in page on a
*separate device or browser* will **not** authenticate the current browser.
External navigation and cookie persistence in the PS5 embedded browser have
not been verified. Archive.org may require additional permission for
individual files. Some links will fail even after signing in.

The public metadata-only catalogue still contains only the **63** public
AlvRo game records. Seven further collections (3,997 files flagged
private in the last metadata audit) are available as directory links, not as
install-ready game records. The optional account feature does **not**
download or install game files.

## Why not a password field or OAuth redirect?

Archive.org's documented restricted-content access uses logged-in browser
cookies. S3 keys serve a different purpose and should not be used as a
replacement for a restricted-download login. A supported third-party OAuth
grant and cross-origin credential flow for Obsidian has not been verified.
Embedding a username/password form, accepting copied session cookies, or
pretending an offsite sign-in authenticates the app would be misleading and
unsafe.

For true authenticated downloads inside the PS5/native application, a
separately reviewed provider-supported delegated auth mechanism, secure
per-device secret storage, CSRF controls, and authenticated availability
checks would be required.

References:
- https://archive.org/services/docs/api/
- https://doc-tools.readthedocs.io/en/ia-test-gsod/tutorial-get-ia-credentials.html
- https://archivesupport.zendesk.com/hc/en-us/articles/360016398872-Downloading-A-Basic-Guide
