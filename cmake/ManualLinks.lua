-- Markdown sources share links; render local HTML links or man-page names.
function Link(link)
    if FORMAT == 'man' then
        return link.content
    end
    link.target = link.target:gsub('%.[17]%.md$', '.html')
    return link
end
